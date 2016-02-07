// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/blink_platform_impl.h"

#include <cmath>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/html_viewer/blink_resource_constants.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/web_clipboard_impl.h"
#include "components/html_viewer/web_cookie_jar_impl.h"
#include "components/html_viewer/web_graphics_context_3d_command_buffer_impl.h"
#include "components/html_viewer/web_socket_handle_impl.h"
#include "components/html_viewer/web_url_loader_impl.h"
#include "components/message_port/web_message_port_channel_impl.h"
#include "components/mime_util/mime_util.h"
#include "components/scheduler/child/webthread_impl_for_worker_scheduler.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/scheduler/renderer/webthread_impl_for_renderer_scheduler.h"
#include "mojo/common/user_agent.h"
#include "mojo/shell/public/cpp/shell.h"
#include "net/base/data_url.h"
#include "net/base/ip_address_number.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebWaitableEvent.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"
#include "url/gurl.h"

namespace html_viewer {
namespace {

// Allows overriding user agent scring.
const char kUserAgentSwitch[] = "user-agent";

class WebWaitableEventImpl : public blink::WebWaitableEvent {
 public:
  WebWaitableEventImpl(ResetPolicy policy, InitialState state) {
    bool manual_reset = policy == ResetPolicy::Manual;
    bool initially_signaled = state == InitialState::Signaled;
    impl_.reset(new base::WaitableEvent(manual_reset, initially_signaled));
  }
  ~WebWaitableEventImpl() override {}

  void reset() override { impl_->Reset(); }
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
    GlobalState* global_state,
    mojo::Shell* shell,
    scheduler::RendererScheduler* renderer_scheduler)
    : global_state_(global_state),
      shell_(shell),
      main_thread_task_runner_(renderer_scheduler->DefaultTaskRunner()),
      main_thread_(renderer_scheduler->CreateMainThread()) {
  if (shell) {
    scoped_ptr<mojo::Connection> connection =
        shell->Connect("mojo:network_service");
    connection->ConnectToService(&web_socket_factory_);
    connection->ConnectToService(&url_loader_factory_);

    mojo::CookieStorePtr cookie_store;
    connection->ConnectToService(&cookie_store);
    cookie_jar_.reset(new WebCookieJarImpl(std::move(cookie_store)));

    mojo::ClipboardPtr clipboard;
    shell->ConnectToService("mojo:clipboard", &clipboard);
    clipboard_.reset(new WebClipboardImpl(std::move(clipboard)));
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

blink::WebString BlinkPlatformImpl::defaultLocale() {
  return blink::WebString::fromUTF8("en-US");
}

blink::WebBlobRegistry* BlinkPlatformImpl::blobRegistry() {
  return &blob_registry_;
}

double BlinkPlatformImpl::currentTimeSeconds() {
  return base::Time::Now().ToDoubleT();
}

double BlinkPlatformImpl::monotonicallyIncreasingTimeSeconds() {
  return base::TimeTicks::Now().ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

bool BlinkPlatformImpl::isThreadedCompositingEnabled() {
  return true;
}

blink::WebCompositorSupport* BlinkPlatformImpl::compositorSupport() {
  return &compositor_support_;
}

uint32_t BlinkPlatformImpl::getUniqueIdForProcess() {
  // TODO(rickyz): Replace this with base::GetUniqueIdForProcess when that's
  // ready.
  return base::trace_event::TraceLog::GetInstance()->process_id();
}

void BlinkPlatformImpl::createMessageChannel(
    blink::WebMessagePortChannel** channel1,
    blink::WebMessagePortChannel** channel2) {
  message_port::WebMessagePortChannelImpl::CreatePair(channel1, channel2);
}

blink::WebScrollbarBehavior* BlinkPlatformImpl::scrollbarBehavior() {
  return &scrollbar_behavior_;
}

blink::WebGraphicsContext3D*
BlinkPlatformImpl::createOffscreenGraphicsContext3D(
    const blink::WebGraphicsContext3D::Attributes& attributes,
    blink::WebGraphicsContext3D* share_context) {
  blink::WebGraphicsContext3D::WebGraphicsInfo gl_info;
  return createOffscreenGraphicsContext3D(attributes, share_context, &gl_info);
}

blink::WebGraphicsContext3D*
BlinkPlatformImpl::createOffscreenGraphicsContext3D(
    const blink::WebGraphicsContext3D::Attributes& attributes,
    blink::WebGraphicsContext3D* share_context,
    blink::WebGraphicsContext3D::WebGraphicsInfo* gl_info) {
  // TODO(penghuang): Use the shell from the right HTMLDocument.
  return WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
      global_state_, shell_, blink::WebStringToGURL(attributes.topDocumentURL),
      attributes, share_context, gl_info);
}

blink::WebGraphicsContext3D*
BlinkPlatformImpl::createOffscreenGraphicsContext3D(
    const blink::WebGraphicsContext3D::Attributes& attributes) {
  return createOffscreenGraphicsContext3D(attributes, nullptr, nullptr);
}

blink::WebGraphicsContext3DProvider*
BlinkPlatformImpl::createSharedOffscreenGraphicsContext3DProvider() {
  return nullptr;
}

blink::WebData BlinkPlatformImpl::loadResource(const char* resource) {
  for (size_t i = 0; i < arraysize(kDataResources); ++i) {
    if (!strcmp(resource, kDataResources[i].name)) {
      base::StringPiece data =
          ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
              kDataResources[i].id, ui::SCALE_FACTOR_100P);
      return blink::WebData(data.data(), data.size());
    }
  }
  NOTREACHED() << "Requested resource is unavailable: " << resource;
  return blink::WebData();
}

blink::WebURLLoader* BlinkPlatformImpl::createURLLoader() {
  return new WebURLLoaderImpl(url_loader_factory_.get(), &blob_registry_);
}

blink::WebSocketHandle* BlinkPlatformImpl::createWebSocketHandle() {
  return new WebSocketHandleImpl(web_socket_factory_.get());
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
  thread->Init();
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

blink::WebWaitableEvent* BlinkPlatformImpl::createWaitableEvent(
    blink::WebWaitableEvent::ResetPolicy policy,
    blink::WebWaitableEvent::InitialState state) {
  return new WebWaitableEventImpl(policy, state);
}

blink::WebWaitableEvent* BlinkPlatformImpl::waitMultipleEvents(
    const blink::WebVector<blink::WebWaitableEvent*>& web_events) {
  std::vector<base::WaitableEvent*> events;
  for (size_t i = 0; i < web_events.size(); ++i)
    events.push_back(static_cast<WebWaitableEventImpl*>(web_events[i])->impl());
  size_t idx = base::WaitableEvent::WaitMany(events.data(), events.size());
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
