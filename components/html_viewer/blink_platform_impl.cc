// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/blink_platform_impl.h"

#include <cmath>

#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/html_viewer/blink_resource_constants.h"
#include "components/html_viewer/web_clipboard_impl.h"
#include "components/html_viewer/web_cookie_jar_impl.h"
#include "components/html_viewer/web_socket_handle_impl.h"
#include "components/html_viewer/web_url_loader_impl.h"
#include "components/message_port/web_message_port_channel_impl.h"
#include "components/mime_util/mime_util.h"
#include "components/scheduler/child/webthread_impl_for_worker_scheduler.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/scheduler/renderer/webthread_impl_for_renderer_scheduler.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/common/user_agent.h"
#include "net/base/data_url.h"
#include "net/base/ip_address_number.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/public/platform/WebWaitableEvent.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"

namespace html_viewer {
namespace {

// Allows overriding user agent scring.
const char kUserAgentSwitch[] = "user-agent";

class WebWaitableEventImpl : public blink::WebWaitableEvent {
 public:
  WebWaitableEventImpl() : impl_(new base::WaitableEvent(false, false)) {}
  ~WebWaitableEventImpl() override {}

  void wait() override { impl_->Wait(); }
  void signal() override { impl_->Signal(); }

  base::WaitableEvent* impl() {
    return impl_.get();
  }

 private:
  scoped_ptr<base::WaitableEvent> impl_;
  DISALLOW_COPY_AND_ASSIGN(WebWaitableEventImpl);
};

}  // namespace

BlinkPlatformImpl::BlinkPlatformImpl(
    mojo::ApplicationImpl* app,
    scheduler::RendererScheduler* renderer_scheduler)
    : main_thread_task_runner_(renderer_scheduler->DefaultTaskRunner()),
      main_thread_(
          new scheduler::WebThreadImplForRendererScheduler(renderer_scheduler)),
      shared_timer_func_(NULL),
      shared_timer_fire_time_(0.0),
      shared_timer_fire_time_was_set_while_suspended_(false),
      shared_timer_suspended_(0) {
  if (app) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:network_service");
    mojo::ApplicationConnection* connection =
        app->ConnectToApplication(request.Pass());
    connection->ConnectToService(&network_service_);
    connection->ConnectToService(&url_loader_factory_);

    mojo::CookieStorePtr cookie_store;
    network_service_->GetCookieStore(GetProxy(&cookie_store));
    cookie_jar_.reset(new WebCookieJarImpl(cookie_store.Pass()));

    mojo::ClipboardPtr clipboard;
    mojo::URLRequestPtr request2(mojo::URLRequest::New());
    request2->url = mojo::String::From("mojo:clipboard");
    app->ConnectToService(request2.Pass(), &clipboard);
    clipboard_.reset(new WebClipboardImpl(clipboard.Pass()));
  }
  shared_timer_.SetTaskRunner(main_thread_task_runner_);
}

BlinkPlatformImpl::~BlinkPlatformImpl() {
}

blink::WebCookieJar* BlinkPlatformImpl::cookieJar() {
  return cookie_jar_.get();
}

blink::WebClipboard* BlinkPlatformImpl::clipboard() {
  return clipboard_.get();
}

blink::WebMimeRegistry* BlinkPlatformImpl::mimeRegistry() {
  return &mime_registry_;
}

blink::WebThemeEngine* BlinkPlatformImpl::themeEngine() {
  return &theme_engine_;
}

blink::WebString BlinkPlatformImpl::defaultLocale() {
  return blink::WebString::fromUTF8("en-US");
}

blink::WebBlobRegistry* BlinkPlatformImpl::blobRegistry() {
  return &blob_registry_;
}

double BlinkPlatformImpl::currentTime() {
  return base::Time::Now().ToDoubleT();
}

double BlinkPlatformImpl::monotonicallyIncreasingTime() {
  return base::TimeTicks::Now().ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

void BlinkPlatformImpl::cryptographicallyRandomValues(unsigned char* buffer,
                                                      size_t length) {
  base::RandBytes(buffer, length);
}

void BlinkPlatformImpl::setSharedTimerFiredFunction(void (*func)()) {
  shared_timer_func_ = func;
}

void BlinkPlatformImpl::setSharedTimerFireInterval(
    double interval_seconds) {
  shared_timer_fire_time_ = interval_seconds + monotonicallyIncreasingTime();
  if (shared_timer_suspended_) {
    shared_timer_fire_time_was_set_while_suspended_ = true;
    return;
  }

  // By converting between double and int64 representation, we run the risk
  // of losing precision due to rounding errors. Performing computations in
  // microseconds reduces this risk somewhat. But there still is the potential
  // of us computing a fire time for the timer that is shorter than what we
  // need.
  // As the event loop will check event deadlines prior to actually firing
  // them, there is a risk of needlessly rescheduling events and of
  // needlessly looping if sleep times are too short even by small amounts.
  // This results in measurable performance degradation unless we use ceil() to
  // always round up the sleep times.
  int64 interval = static_cast<int64>(
      ceil(interval_seconds * base::Time::kMillisecondsPerSecond)
      * base::Time::kMicrosecondsPerMillisecond);

  if (interval < 0)
    interval = 0;

  shared_timer_.Stop();
  shared_timer_.Start(FROM_HERE, base::TimeDelta::FromMicroseconds(interval),
                      this, &BlinkPlatformImpl::DoTimeout);
}

void BlinkPlatformImpl::stopSharedTimer() {
  shared_timer_.Stop();
}

bool BlinkPlatformImpl::isThreadedCompositingEnabled() {
  return true;
}

blink::WebCompositorSupport* BlinkPlatformImpl::compositorSupport() {
  return &compositor_support_;
}

void BlinkPlatformImpl::createMessageChannel(
    blink::WebMessagePortChannel** channel1,
    blink::WebMessagePortChannel** channel2) {
  message_port::WebMessagePortChannelImpl::CreatePair(channel1, channel2);
}

blink::WebScrollbarBehavior* BlinkPlatformImpl::scrollbarBehavior() {
  return &scrollbar_behavior_;
}

const unsigned char* BlinkPlatformImpl::getTraceCategoryEnabledFlag(
    const char* category_name) {
  static const unsigned char buf[] = "*";
  return buf;
}

blink::WebData BlinkPlatformImpl::loadResource(const char* resource) {
  for (size_t i = 0; i < arraysize(kDataResources); ++i) {
    if (!strcmp(resource, kDataResources[i].name)) {
      int length;
      const unsigned char* data =
          blink_resource_map_.GetResource(kDataResources[i].id, &length);
      CHECK(data != nullptr && length > 0);
      return blink::WebData(reinterpret_cast<const char*>(data), length);
    }
  }
  NOTREACHED() << "Requested resource is unavailable: " << resource;
  return blink::WebData();
}

blink::WebURLLoader* BlinkPlatformImpl::createURLLoader() {
  return new WebURLLoaderImpl(url_loader_factory_.get(), &blob_registry_);
}

blink::WebSocketHandle* BlinkPlatformImpl::createWebSocketHandle() {
  return new WebSocketHandleImpl(network_service_.get());
}

blink::WebString BlinkPlatformImpl::userAgent() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUserAgentSwitch)) {
    return blink::WebString::fromUTF8(
        command_line->GetSwitchValueASCII(kUserAgentSwitch));
  }
  return blink::WebString::fromUTF8(mojo::common::GetUserAgent());
}

blink::WebData BlinkPlatformImpl::parseDataURL(
    const blink::WebURL& url,
    blink::WebString& mimetype_out,
    blink::WebString& charset_out) {
  std::string mimetype, charset, data;
  if (net::DataURL::Parse(url, &mimetype, &charset, &data) &&
      mime_util::IsSupportedMimeType(mimetype)) {
    mimetype_out = blink::WebString::fromUTF8(mimetype);
    charset_out = blink::WebString::fromUTF8(charset);
    return data;
  }
  return blink::WebData();
}

blink::WebURLError BlinkPlatformImpl::cancelledError(const blink::WebURL& url)
    const {
  blink::WebURLError error;
  error.domain = blink::WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_ABORTED;
  error.unreachableURL = url;
  error.staleCopyInCache = false;
  error.isCancellation = true;
  return error;
}

bool BlinkPlatformImpl::isReservedIPAddress(
    const blink::WebString& host) const {
  net::IPAddressNumber address;
  if (!net::ParseURLHostnameToNumber(host.utf8(), &address))
    return false;
  return net::IsIPAddressReserved(address);
}

blink::WebThread* BlinkPlatformImpl::createThread(const char* name) {
  scheduler::WebThreadImplForWorkerScheduler* thread =
      new scheduler::WebThreadImplForWorkerScheduler(name);
  thread->TaskRunner()->PostTask(
      FROM_HERE, base::Bind(&BlinkPlatformImpl::UpdateWebThreadTLS,
                            base::Unretained(this), thread));
  return thread;
}

blink::WebThread* BlinkPlatformImpl::currentThread() {
  if (main_thread_->isCurrentThread())
    return main_thread_.get();
  return static_cast<blink::WebThread*>(current_thread_slot_.Get());
}

void BlinkPlatformImpl::yieldCurrentThread() {
  base::PlatformThread::YieldCurrentThread();
}

blink::WebWaitableEvent* BlinkPlatformImpl::createWaitableEvent() {
  return new WebWaitableEventImpl();
}

blink::WebWaitableEvent* BlinkPlatformImpl::waitMultipleEvents(
    const blink::WebVector<blink::WebWaitableEvent*>& web_events) {
  std::vector<base::WaitableEvent*> events;
  for (size_t i = 0; i < web_events.size(); ++i)
    events.push_back(static_cast<WebWaitableEventImpl*>(web_events[i])->impl());
  size_t idx = base::WaitableEvent::WaitMany(
      vector_as_array(&events), events.size());
  DCHECK_LT(idx, web_events.size());
  return web_events[idx];
}

blink::WebGestureCurve* BlinkPlatformImpl::createFlingAnimationCurve(
    blink::WebGestureDevice device_source,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll) {
  const bool is_main_thread = true;
  return ui::WebGestureCurveImpl::CreateFromDefaultPlatformCurve(
             gfx::Vector2dF(velocity.x, velocity.y),
             gfx::Vector2dF(cumulative_scroll.width, cumulative_scroll.height),
             is_main_thread).release();
}

blink::WebCrypto* BlinkPlatformImpl::crypto() {
  return &web_crypto_;
}

blink::WebNotificationManager*
BlinkPlatformImpl::notificationManager() {
  return &web_notification_manager_;
}

void BlinkPlatformImpl::UpdateWebThreadTLS(blink::WebThread* thread) {
  DCHECK(!current_thread_slot_.Get());
  current_thread_slot_.Set(thread);
}

}  // namespace html_viewer
