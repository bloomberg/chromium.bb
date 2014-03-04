// // Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBKITPLATFORMSUPPORT_CHILD_IMPL_H_
#define CONTENT_CHILD_WEBKITPLATFORMSUPPORT_CHILD_IMPL_H_

#include "base/threading/thread_local_storage.h"
#include "content/child/blink_platform_impl.h"
#include "content/child/webfallbackthemeengine_impl.h"
#include "content/common/content_export.h"

#if defined(USE_DEFAULT_RENDER_THEME)
#include "content/child/webthemeengine_impl_default.h"
#elif defined(OS_WIN)
#include "content/child/webthemeengine_impl_win.h"
#elif defined(OS_MACOSX)
#include "content/child/webthemeengine_impl_mac.h"
#elif defined(OS_ANDROID)
#include "content/child/webthemeengine_impl_android.h"
#endif

namespace content {

class FlingCurveConfiguration;

class CONTENT_EXPORT WebKitPlatformSupportChildImpl : public BlinkPlatformImpl {
 public:
  WebKitPlatformSupportChildImpl();
  virtual ~WebKitPlatformSupportChildImpl();

  // Platform methods (partial implementation):
  virtual blink::WebThemeEngine* themeEngine();
  virtual blink::WebFallbackThemeEngine* fallbackThemeEngine();

  void SetFlingCurveParameters(
    const std::vector<float>& new_touchpad,
    const std::vector<float>& new_touchscreen);

  virtual blink::WebGestureCurve* createFlingAnimationCurve(
      int device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll) OVERRIDE;

  virtual blink::WebThread* createThread(const char* name);
  virtual blink::WebThread* currentThread();

  virtual blink::WebWaitableEvent* createWaitableEvent();
  virtual blink::WebWaitableEvent* waitMultipleEvents(
      const blink::WebVector<blink::WebWaitableEvent*>& events);

  virtual void didStartWorkerRunLoop(
      const blink::WebWorkerRunLoop& runLoop) OVERRIDE;
  virtual void didStopWorkerRunLoop(
      const blink::WebWorkerRunLoop& runLoop) OVERRIDE;

  virtual blink::WebDiscardableMemory* allocateAndLockDiscardableMemory(
      size_t bytes);

 private:
  static void DestroyCurrentThread(void*);

  WebThemeEngineImpl native_theme_engine_;
  WebFallbackThemeEngineImpl fallback_theme_engine_;
  base::ThreadLocalStorage::Slot current_thread_slot_;
  scoped_ptr<FlingCurveConfiguration> fling_curve_configuration_;
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBKITPLATFORMSUPPORT_CHILD_IMPL_H_
