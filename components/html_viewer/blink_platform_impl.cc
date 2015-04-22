// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/blink_platform_impl.h"

#include <cmath>

#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/html_viewer/blink_resource_constants.h"
#include "components/html_viewer/web_clipboard_impl.h"
#include "components/html_viewer/web_cookie_jar_impl.h"
#include "components/html_viewer/web_message_port_channel_impl.h"
#include "components/html_viewer/web_socket_handle_impl.h"
#include "components/html_viewer/web_thread_impl.h"
#include "components/html_viewer/web_url_loader_impl.h"
#include "net/base/data_url.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/public/platform/WebWaitableEvent.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_impl.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"

namespace html_viewer {
namespace {

// Allows overriding user agent scring.
const char kUserAgentSwitch[] = "user-agent";

// TODO(darin): Figure out what our UA should really be.
const char kDefaultUserAgentString[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/42.0.2311.68 Safari/537.36";

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

BlinkPlatformImpl::BlinkPlatformImpl(mojo::ApplicationImpl* app)
    : main_loop_(base::MessageLoop::current()),
      shared_timer_func_(NULL),
      shared_timer_fire_time_(0.0),
      shared_timer_fire_time_was_set_while_suspended_(false),
      shared_timer_suspended_(0),
      current_thread_slot_(&DestroyCurrentThread),
      scheduler_(main_loop_->message_loop_proxy()) {
  if (app) {
    app->ConnectToService("mojo:network_service", &network_service_);

    mojo::CookieStorePtr cookie_store;
    network_service_->GetCookieStore(GetProxy(&cookie_store));
    cookie_jar_.reset(new WebCookieJarImpl(cookie_store.Pass()));

    mojo::ClipboardPtr clipboard;
    app->ConnectToService("mojo:clipboard", &clipboard);
    clipboard_.reset(new WebClipboardImpl(clipboard.Pass()));
  }
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

blink::WebScheduler* BlinkPlatformImpl::scheduler() {
  return &scheduler_;
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

void BlinkPlatformImpl::callOnMainThread(
    void (*func)(void*), void* context) {
  main_loop_->PostTask(FROM_HERE, base::Bind(func, context));
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
  WebMessagePortChannelImpl::CreatePair(channel1, channel2);
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
  return new WebURLLoaderImpl(network_service_.get(), &blob_registry_);
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
  return blink::WebString::fromUTF8(kDefaultUserAgentString);
}

blink::WebData BlinkPlatformImpl::parseDataURL(
    const blink::WebURL& url,
    blink::WebString& mimetype_out,
    blink::WebString& charset_out) {
  std::string mimetype, charset, data;
  if (net::DataURL::Parse(url, &mimetype, &charset, &data)
      && net::IsSupportedMimeType(mimetype)) {
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
  return new WebThreadImpl(name);
}

blink::WebThread* BlinkPlatformImpl::currentThread() {
  WebThreadImplForMessageLoop* thread =
      static_cast<WebThreadImplForMessageLoop*>(current_thread_slot_.Get());
  if (thread)
    return (thread);

  scoped_refptr<base::MessageLoopProxy> message_loop =
      base::MessageLoopProxy::current();
  if (!message_loop.get())
    return NULL;

  thread = new WebThreadImplForMessageLoop(message_loop.get());
  current_thread_slot_.Set(thread);
  return thread;
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

// static
void BlinkPlatformImpl::DestroyCurrentThread(void* thread) {
  WebThreadImplForMessageLoop* impl =
      static_cast<WebThreadImplForMessageLoop*>(thread);
  delete impl;
}

}  // namespace html_viewer
