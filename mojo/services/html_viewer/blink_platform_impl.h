// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_BLINK_PLATFORM_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_BLINK_PLATFORM_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_local_storage.h"
#include "base/timer/timer.h"
#include "cc/blink/web_compositor_support_impl.h"
#include "mojo/services/html_viewer/blink_resource_map.h"
#include "mojo/services/html_viewer/webmimeregistry_impl.h"
#include "mojo/services/html_viewer/webscheduler_impl.h"
#include "mojo/services/html_viewer/webthemeengine_impl.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebScrollbarBehavior.h"

namespace html_viewer {

class BlinkPlatformImpl : public blink::Platform {
 public:
  explicit BlinkPlatformImpl();
  ~BlinkPlatformImpl() override;

  // blink::Platform methods:
  blink::WebMimeRegistry* mimeRegistry() override;
  blink::WebThemeEngine* themeEngine() override;
  blink::WebScheduler* scheduler() override;
  blink::WebString defaultLocale() override;
  double currentTime() override;
  double monotonicallyIncreasingTime() override;
  void cryptographicallyRandomValues(unsigned char* buffer,
                                     size_t length) override;
  void setSharedTimerFiredFunction(void (*func)()) override;
  void setSharedTimerFireInterval(double interval_seconds) override;
  void stopSharedTimer() override;
  virtual void callOnMainThread(void (*func)(void*), void* context);
  bool isThreadedCompositingEnabled() override;
  blink::WebCompositorSupport* compositorSupport() override;
  blink::WebURLLoader* createURLLoader() override;
  blink::WebSocketHandle* createWebSocketHandle() override;
  blink::WebString userAgent() override;
  blink::WebData parseDataURL(
      const blink::WebURL& url, blink::WebString& mime_type,
      blink::WebString& charset) override;
  bool isReservedIPAddress(const blink::WebString& host) const override;
  blink::WebURLError cancelledError(const blink::WebURL& url) const override;
  blink::WebThread* createThread(const char* name) override;
  blink::WebThread* currentThread() override;
  void yieldCurrentThread() override;
  blink::WebWaitableEvent* createWaitableEvent() override;
  blink::WebWaitableEvent* waitMultipleEvents(
      const blink::WebVector<blink::WebWaitableEvent*>& events) override;
  blink::WebScrollbarBehavior* scrollbarBehavior() override;
  const unsigned char* getTraceCategoryEnabledFlag(
      const char* category_name) override;
  blink::WebData loadResource(const char* name) override;
  blink::WebGestureCurve* createFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll) override;

 private:
  void SuspendSharedTimer();
  void ResumeSharedTimer();

  void DoTimeout() {
    if (shared_timer_func_ && !shared_timer_suspended_)
      shared_timer_func_();
  }

  static void DestroyCurrentThread(void*);

  base::MessageLoop* main_loop_;
  base::OneShotTimer<BlinkPlatformImpl> shared_timer_;
  void (*shared_timer_func_)();
  double shared_timer_fire_time_;
  bool shared_timer_fire_time_was_set_while_suspended_;
  int shared_timer_suspended_;  // counter
  base::ThreadLocalStorage::Slot current_thread_slot_;
  cc_blink::WebCompositorSupportImpl compositor_support_;
  WebThemeEngineImpl theme_engine_;
  WebMimeRegistryImpl mime_registry_;
  WebSchedulerImpl scheduler_;
  blink::WebScrollbarBehavior scrollbar_behavior_;
  BlinkResourceMap blink_resource_map_;

  DISALLOW_COPY_AND_ASSIGN(BlinkPlatformImpl);
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_BLINK_PLATFORM_IMPL_H_
