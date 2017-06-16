// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_H_

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
#include "content/browser/android/content_view_core_impl_observer.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/android/content_view_core.h"
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

class GinJavaBridgeDispatcherHost;
class RenderFrameHost;
class RenderWidgetHostViewAndroid;
struct MenuItem;

class ContentViewCoreImpl : public ContentViewCore,
                            public WebContentsObserver {
 public:
  static ContentViewCoreImpl* FromWebContents(WebContents* web_contents);
  ContentViewCoreImpl(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj,
      WebContents* web_contents,
      float dpi_scale,
      const base::android::JavaRef<jobject>& java_bridge_retained_object_set);

  // ContentViewCore implementation.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  WebContents* GetWebContents() const override;
  ui::WindowAndroid* GetWindowAndroid() const override;

  void AddObserver(ContentViewCoreImplObserver* observer);
  void RemoveObserver(ContentViewCoreImplObserver* observer);

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  base::android::ScopedJavaLocalRef<jobject> GetWebContentsAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jobject> GetJavaWindowAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void UpdateWindowAndroid(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jlong window_android);
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

  void ResolveTapDisambiguation(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                jlong time_ms,
                                jfloat x,
                                jfloat y,
                                jboolean is_long_press);

  void PinchBegin(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  jlong time_ms,
                  jfloat x,
                  jfloat y);
  void PinchEnd(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jlong time_ms);
  void PinchBy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jlong time_ms,
               jfloat x,
               jfloat y,
               jfloat delta);
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
  void SetAllowJavascriptInterfacesInspection(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean allow);
  void AddJavascriptInterface(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jstring>& name,
      const base::android::JavaParamRef<jclass>& safe_annotation_clazz);
  void RemoveJavascriptInterface(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& name);
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
                       const gfx::Vector2dF& page_scale_factor_limits,
                       const gfx::SizeF& content_size,
                       const gfx::SizeF& viewport_size,
                       const float content_offset,
                       const float top_shown_pix,
                       bool top_changed,
                       bool is_mobile_optimized_hint);

  bool HasFocus();
  void RequestDisallowInterceptTouchEvent();
  void OnGestureEventAck(const blink::WebGestureEvent& event,
                         InputEventAckState ack_result);
  bool FilterInputEvent(const blink::WebInputEvent& event);

  // Shows the disambiguation popup
  // |rect_pixels|   --> window coordinates which |zoomed_bitmap| represents
  // |zoomed_bitmap| --> magnified image of potential touch targets
  void ShowDisambiguationPopup(
      const gfx::Rect& rect_pixels, const SkBitmap& zoomed_bitmap);

  // Creates a java-side touch event, used for injecting motion events for
  // testing/benchmarking purposes.
  base::android::ScopedJavaLocalRef<jobject> CreateMotionEventSynthesizer();

  void DidStopFlinging();

  // Returns the context with which the ContentViewCore was created, typically
  // the Activity context.
  base::android::ScopedJavaLocalRef<jobject> GetContext() const;

  // Returns the viewport size after accounting for the viewport offset.
  gfx::Size GetViewSize() const;

  bool IsFullscreenRequiredForOrientationLock() const;

  // --------------------------------------------------------------------------
  // Methods called from native code
  // --------------------------------------------------------------------------

  gfx::Size GetViewportSizeDip() const;
  bool DoBrowserControlsShrinkBlinkSize() const;
  float GetTopControlsHeightDip() const;
  float GetBottomControlsHeightDip() const;

  void MoveRangeSelectionExtent(const gfx::PointF& extent);

  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent);

  void OnTouchDown(const base::android::ScopedJavaLocalRef<jobject>& event);

  ui::ViewAndroid* GetViewAndroid() const;

 private:
  class ContentViewUserData;

  friend class ContentViewUserData;
  ~ContentViewCoreImpl() override;

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
  int GetTopControlsHeightPix() const;
  int GetBottomControlsHeightPix() const;

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

  // Page scale factor.
  float page_scale_;

  // Device scale factor.
  float dpi_scale_;

  // Observer to notify of lifecyle changes.
  base::ObserverList<ContentViewCoreImplObserver> observer_list_;

  // The cache of device's current orientation set from Java side, this value
  // will be sent to Renderer once it is ready.
  int device_orientation_;

  // Manages injecting Java objects.
  scoped_refptr<GinJavaBridgeDispatcherHost> java_bridge_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(ContentViewCoreImpl);
};

bool RegisterContentViewCore(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_H_
