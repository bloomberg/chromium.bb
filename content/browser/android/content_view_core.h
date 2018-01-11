// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/selection_bound.h"
#include "url/gurl.h"

namespace ui {
class WindowAndroid;
}

namespace content {

class RenderFrameHost;
class RenderWidgetHostViewAndroid;
struct MenuItem;

class ContentViewCore : public WebContentsObserver {
 public:
  static ContentViewCore* FromWebContents(WebContents* web_contents);
  ContentViewCore(JNIEnv* env,
                  const base::android::JavaRef<jobject>& obj,
                  WebContents* web_contents,
                  float dpi_scale);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
  WebContents* GetWebContents() const;
  ui::WindowAndroid* GetWindowAndroid() const;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  base::android::ScopedJavaLocalRef<jobject> GetWebContentsAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jobject> GetJavaWindowAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void UpdateWindowAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jwindow_android);
  void OnJavaContentViewCoreDestroyed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Notifies the ContentViewCore that items were selected in the currently
  // showing select popup.
  void SelectPopupMenuItems(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong selectPopupSourceFrame,
      const base::android::JavaParamRef<jintArray>& indices);

  // Returns the amount of the top controls height if controls are in the state
  // of shrinking Blink's view size, otherwise 0.
  int GetTopControlsShrinkBlinkHeightPixForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void SendOrientationChangeEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint orientation);
  jboolean SendMouseWheelEvent(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               jlong time_ms,
                               jfloat x,
                               jfloat y,
                               jfloat ticks_x,
                               jfloat ticks_y,
                               jfloat pixels_per_tick);
  void ScrollBegin(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jlong time_ms,
                   jfloat x,
                   jfloat y,
                   jfloat hintx,
                   jfloat hinty,
                   jboolean target_viewport,
                   jboolean from_gamepad);
  void ScrollEnd(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj,
                 jlong time_ms);
  void ScrollBy(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jlong time_ms,
                jfloat x,
                jfloat y,
                jfloat dx,
                jfloat dy);
  void FlingStart(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  jlong time_ms,
                  jfloat x,
                  jfloat y,
                  jfloat vx,
                  jfloat vy,
                  jboolean target_viewport,
                  jboolean from_gamepad);
  void FlingCancel(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jlong time_ms,
                   jboolean from_gamepad);
  void DoubleTap(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj,
                 jlong time_ms,
                 jfloat x,
                 jfloat y);

  void SetTextHandlesTemporarilyHidden(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean hidden);

  void ResetGestureDetection(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);
  void SetDoubleTapSupportEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean enabled);
  void SetMultiTouchZoomSupportEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean enabled);

  void SetFocus(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jboolean focused);

  void SetDIPScale(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jfloat dipScale);

  jint GetBackgroundColor(JNIEnv* env, jobject obj);
  void WasResized(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void SetTextTrackSettings(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean textTracksEnabled,
      const base::android::JavaParamRef<jstring>& textTrackBackgroundColor,
      const base::android::JavaParamRef<jstring>& textTrackFontFamily,
      const base::android::JavaParamRef<jstring>& textTrackFontStyle,
      const base::android::JavaParamRef<jstring>& textTrackFontVariant,
      const base::android::JavaParamRef<jstring>& textTrackTextColor,
      const base::android::JavaParamRef<jstring>& textTrackTextShadow,
      const base::android::JavaParamRef<jstring>& textTrackTextSize);

  void SetBackgroundOpaque(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jobj,
                           jboolean opaque);

  jint GetCurrentRenderProcessId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  jboolean UsingSynchronousCompositing(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // --------------------------------------------------------------------------
  // Public methods that call to Java via JNI
  // --------------------------------------------------------------------------

  void HidePopupsAndPreserveSelection();

  // Creates a popup menu with |items|.
  // |multiple| defines if it should support multi-select.
  // If not |multiple|, |selected_item| sets the initially selected item.
  // Otherwise, item's "checked" flag selects it.
  void ShowSelectPopupMenu(RenderFrameHost* frame,
                           const gfx::Rect& bounds,
                           const std::vector<MenuItem>& items,
                           int selected_item,
                           bool multiple,
                           bool right_aligned);
  // Hides a visible popup menu.
  void HideSelectPopupMenu();

  // All sizes and offsets are in CSS pixels (except |top_show_pix|)
  // as cached by the renderer.
  void UpdateFrameInfo(const gfx::Vector2dF& scroll_offset,
                       float page_scale_factor,
                       float min_page_scale,
                       float max_page_scale,
                       const gfx::SizeF& content_size,
                       const gfx::SizeF& viewport_size,
                       const float top_content_offset,
                       const float top_shown_pix,
                       bool top_changed,
                       bool is_mobile_optimized_hint);

  void RequestDisallowInterceptTouchEvent();
  bool FilterInputEvent(const blink::WebInputEvent& event);

  void DidStopFlinging();

  // Returns the context with which the ContentViewCore was created, typically
  // the Activity context.
  base::android::ScopedJavaLocalRef<jobject> GetContext() const;

  bool IsFullscreenRequiredForOrientationLock() const;

  // --------------------------------------------------------------------------
  // Methods called from native code
  // --------------------------------------------------------------------------

  int GetMouseWheelMinimumGranularity() const;

  void UpdateCursor(const content::CursorInfo& info);
  void OnTouchDown(const base::android::ScopedJavaLocalRef<jobject>& event);

  ui::ViewAndroid* GetViewAndroid() const;

 private:
  class ContentViewUserData;

  friend class ContentViewUserData;
  ~ContentViewCore() override;

  // WebContentsObserver implementation.
  void RenderViewReady() override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;
  void WebContentsDestroyed() override;

  // --------------------------------------------------------------------------
  // Other private methods and data
  // --------------------------------------------------------------------------

  void InitWebContents();
  void SendScreenRectsAndResizeWidget();

  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid() const;

  blink::WebGestureEvent MakeGestureEvent(blink::WebInputEvent::Type type,
                                          int64_t time_ms,
                                          float x,
                                          float y) const;

  gfx::Size GetViewportSizePix() const;

  void SendGestureEvent(const blink::WebGestureEvent& event);

  // Update focus state of the RenderWidgetHostView.
  void SetFocusInternal(bool focused);

  // Send device_orientation_ to renderer.
  void SendOrientationChangeEventInternal();

  float dpi_scale() const { return dpi_scale_; }

  // A weak reference to the Java ContentViewCore object.
  JavaObjectWeakGlobalRef java_ref_;

  // Select popup view
  ui::ViewAndroid::ScopedAnchorView select_popup_;

  // Reference to the current WebContents used to determine how and what to
  // display in the ContentViewCore.
  WebContentsImpl* web_contents_;

  // Device scale factor.
  float dpi_scale_;

  // The cache of device's current orientation set from Java side, this value
  // will be sent to Renderer once it is ready.
  int device_orientation_;

  DISALLOW_COPY_AND_ASSIGN(ContentViewCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
