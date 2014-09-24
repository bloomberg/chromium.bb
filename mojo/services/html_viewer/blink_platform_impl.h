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
#include "mojo/services/html_viewer/webmimeregistry_impl.h"
#include "mojo/services/html_viewer/webthemeengine_impl.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebScrollbarBehavior.h"

namespace mojo {
class ApplicationImpl;
class WebClipboardImpl;
class WebCookieJarImpl;

class BlinkPlatformImpl : public blink::Platform {
 public:
  explicit BlinkPlatformImpl(ApplicationImpl* app);
  virtual ~BlinkPlatformImpl();

  // blink::Platform methods:
  virtual blink::WebCookieJar* cookieJar();
  virtual blink::WebClipboard* clipboard();
  virtual blink::WebMimeRegistry* mimeRegistry();
  virtual blink::WebThemeEngine* themeEngine();
  virtual blink::WebString defaultLocale();
  virtual double currentTime();
  virtual double monotonicallyIncreasingTime();
  virtual void cryptographicallyRandomValues(
      unsigned char* buffer, size_t length);
  virtual void setSharedTimerFiredFunction(void (*func)());
  virtual void setSharedTimerFireInterval(double interval_seconds);
  virtual void stopSharedTimer();
  virtual void callOnMainThread(void (*func)(void*), void* context);
  virtual bool isThreadedCompositingEnabled();
  virtual blink::WebCompositorSupport* compositorSupport();
  virtual blink::WebURLLoader* createURLLoader();
  virtual blink::WebSocketHandle* createWebSocketHandle();
  virtual blink::WebString userAgent();
  virtual blink::WebData parseDataURL(
      const blink::WebURL& url, blink::WebString& mime_type,
      blink::WebString& charset);
  virtual blink::WebURLError cancelledError(const blink::WebURL& url) const;
  virtual blink::WebThread* createThread(const char* name);
  virtual blink::WebThread* currentThread();
  virtual void yieldCurrentThread();
  virtual blink::WebWaitableEvent* createWaitableEvent();
  virtual blink::WebWaitableEvent* waitMultipleEvents(
      const blink::WebVector<blink::WebWaitableEvent*>& events);
  virtual blink::WebScrollbarBehavior* scrollbarBehavior();
  virtual const unsigned char* getTraceCategoryEnabledFlag(
      const char* category_name);

 private:
  void SuspendSharedTimer();
  void ResumeSharedTimer();

  void DoTimeout() {
    if (shared_timer_func_ && !shared_timer_suspended_)
      shared_timer_func_();
  }

  static void DestroyCurrentThread(void*);

  NetworkServicePtr network_service_;
  base::MessageLoop* main_loop_;
  base::OneShotTimer<BlinkPlatformImpl> shared_timer_;
  void (*shared_timer_func_)();
  double shared_timer_fire_time_;
  bool shared_timer_fire_time_was_set_while_suspended_;
  int shared_timer_suspended_;  // counter
  base::ThreadLocalStorage::Slot current_thread_slot_;
  cc_blink::WebCompositorSupportImpl compositor_support_;
  WebThemeEngineImpl theme_engine_;
  scoped_ptr<WebCookieJarImpl> cookie_jar_;
  scoped_ptr<WebClipboardImpl> clipboard_;
  WebMimeRegistryImpl mime_registry_;
  blink::WebScrollbarBehavior scrollbar_behavior_;

  DISALLOW_COPY_AND_ASSIGN(BlinkPlatformImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_HTML_VIEWER_BLINK_PLATFORM_IMPL_H_
