// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "url/gurl.h"

namespace ui {
class ViewAndroid;
class WindowAndroid;
}

namespace content {
class GinJavaBridgeDispatcherHost;
class RenderWidgetHostViewAndroid;
struct MenuItem;

// TODO(jrg): this is a shell.  Upstream the rest.
class ContentViewCoreImpl : public ContentViewCore,
                            public WebContentsObserver {
 public:
  static ContentViewCoreImpl* FromWebContents(WebContents* web_contents);
  ContentViewCoreImpl(JNIEnv* env,
                      jobject obj,
                      WebContents* web_contents,
                      ui::ViewAndroid* view_android,
                      ui::WindowAndroid* window_android,
                      jobject java_bridge_retained_object_set);

  // ContentViewCore implementation.
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() OVERRIDE;
  virtual WebContents* GetWebContents() const OVERRIDE;
  virtual ui::ViewAndroid* GetViewAndroid() const OVERRIDE;
  virtual ui::WindowAndroid* GetWindowAndroid() const OVERRIDE;
  virtual scoped_refptr<cc::Layer> GetLayer() const OVERRIDE;
  virtual void LoadUrl(NavigationController::LoadURLParams& params) OVERRIDE;
  virtual void ShowPastePopup(int x, int y) OVERRIDE;
  virtual void GetScaledContentBitmap(
      float scale,
      SkColorType color_type,
      gfx::Rect src_subrect,
      const base::Callback<void(bool, const SkBitmap&)>& result_callback)
      OVERRIDE;
  virtual float GetDpiScale() const OVERRIDE;
  virtual void PauseVideo() OVERRIDE;
  virtual void PauseOrResumeGeolocation(bool should_pause) OVERRIDE;
  virtual void RequestTextSurroundingSelection(
      int max_length,
      const base::Callback<void(const base::string16& content,
                                int start_offset,
                                int end_offset)>& callback) OVERRIDE;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  base::android::ScopedJavaLocalRef<jobject> GetWebContentsAndroid(JNIEnv* env,
                                                                   jobject obj);

  void OnJavaContentViewCoreDestroyed(JNIEnv* env, jobject obj);

  // Notifies the ContentViewCore that items were selected in the currently
  // showing select popup.
  void SelectPopupMenuItems(JNIEnv* env, jobject obj, jintArray indices);

  void LoadUrl(
      JNIEnv* env, jobject obj,
      jstring url,
      jint load_url_type,
      jint transition_type,
      jstring j_referrer_url,
      jint referrer_policy,
      jint ua_override_option,
      jstring extra_headers,
      jbyteArray post_data,
      jstring base_url_for_data_url,
      jstring virtual_url_for_data_url,
      jboolean can_load_local_resources,
      jboolean is_renderer_initiated);
  base::android::ScopedJavaLocalRef<jstring> GetURL(JNIEnv* env, jobject) const;
  jboolean IsIncognito(JNIEnv* env, jobject obj);
  void SendOrientationChangeEvent(JNIEnv* env, jobject obj, jint orientation);
  jboolean OnTouchEvent(JNIEnv* env,
                        jobject obj,
                        jobject motion_event,
                        jlong time_ms,
                        jint android_action,
                        jint pointer_count,
                        jint history_size,
                        jint action_index,
                        jfloat pos_x_0,
                        jfloat pos_y_0,
                        jfloat pos_x_1,
                        jfloat pos_y_1,
                        jint pointer_id_0,
                        jint pointer_id_1,
                        jfloat touch_major_0,
                        jfloat touch_major_1,
                        jfloat raw_pos_x,
                        jfloat raw_pos_y,
                        jint android_tool_type_0,
                        jint android_tool_type_1,
                        jint android_button_state,
                        jboolean is_touch_handle_event);
  jboolean SendMouseMoveEvent(JNIEnv* env,
                              jobject obj,
                              jlong time_ms,
                              jfloat x,
                              jfloat y);
  jboolean SendMouseWheelEvent(JNIEnv* env,
                               jobject obj,
                               jlong time_ms,
                               jfloat x,
                               jfloat y,
                               jfloat vertical_axis);
  void ScrollBegin(JNIEnv* env, jobject obj, jlong time_ms,
                   jfloat x, jfloat y, jfloat hintx, jfloat hinty);
  void ScrollEnd(JNIEnv* env, jobject obj, jlong time_ms);
  void ScrollBy(JNIEnv* env, jobject obj, jlong time_ms,
                jfloat x, jfloat y, jfloat dx, jfloat dy);
  void FlingStart(JNIEnv* env, jobject obj, jlong time_ms,
                  jfloat x, jfloat y, jfloat vx, jfloat vy);
  void FlingCancel(JNIEnv* env, jobject obj, jlong time_ms);
  void SingleTap(JNIEnv* env, jobject obj, jlong time_ms,
                 jfloat x, jfloat y);
  void DoubleTap(JNIEnv* env, jobject obj, jlong time_ms,
                 jfloat x, jfloat y) ;
  void LongPress(JNIEnv* env, jobject obj, jlong time_ms,
                 jfloat x, jfloat y);
  void PinchBegin(JNIEnv* env, jobject obj, jlong time_ms, jfloat x, jfloat y);
  void PinchEnd(JNIEnv* env, jobject obj, jlong time_ms);
  void PinchBy(JNIEnv* env, jobject obj, jlong time_ms,
               jfloat x, jfloat y, jfloat delta);
  void SelectBetweenCoordinates(JNIEnv* env, jobject obj,
                                jfloat x1, jfloat y1,
                                jfloat x2, jfloat y2);
  void MoveCaret(JNIEnv* env, jobject obj, jfloat x, jfloat y);
  void HideTextHandles(JNIEnv* env, jobject obj);

  void ResetGestureDetection(JNIEnv* env, jobject obj);
  void SetDoubleTapSupportEnabled(JNIEnv* env, jobject obj, jboolean enabled);
  void SetMultiTouchZoomSupportEnabled(JNIEnv* env,
                                       jobject obj,
                                       jboolean enabled);

  void ClearHistory(JNIEnv* env, jobject obj);
  void EvaluateJavaScript(JNIEnv* env,
                          jobject obj,
                          jstring script,
                          jobject callback,
                          jboolean start_renderer);
  void PostMessageToFrame(JNIEnv* env, jobject obj, jstring frame_id,
      jstring message, jstring source_origin, jstring target_origin);
  long GetNativeImeAdapter(JNIEnv* env, jobject obj);
  void SetFocus(JNIEnv* env, jobject obj, jboolean focused);

  jint GetBackgroundColor(JNIEnv* env, jobject obj);
  void SetBackgroundColor(JNIEnv* env, jobject obj, jint color);
  void ClearSslPreferences(JNIEnv* env, jobject /* obj */);
  void SetUseDesktopUserAgent(JNIEnv* env,
                              jobject /* obj */,
                              jboolean state,
                              jboolean reload_on_state_change);
  bool GetUseDesktopUserAgent(JNIEnv* env, jobject /* obj */);
  void SetAllowJavascriptInterfacesInspection(JNIEnv* env,
                                              jobject obj,
                                              jboolean allow);
  void AddJavascriptInterface(JNIEnv* env,
                              jobject obj,
                              jobject object,
                              jstring name,
                              jclass safe_annotation_clazz);
  void RemoveJavascriptInterface(JNIEnv* env, jobject obj, jstring name);
  int GetNavigationHistory(JNIEnv* env, jobject obj, jobject history);
  void GetDirectedNavigationHistory(JNIEnv* env,
                                    jobject obj,
                                    jobject history,
                                    jboolean is_forward,
                                    jint max_entries);
  base::android::ScopedJavaLocalRef<jstring>
      GetOriginalUrlForActiveNavigationEntry(JNIEnv* env, jobject obj);
  void WasResized(JNIEnv* env, jobject obj);

  void SetAccessibilityEnabled(JNIEnv* env, jobject obj, bool enabled);

  void ExtractSmartClipData(JNIEnv* env,
                            jobject obj,
                            jint x,
                            jint y,
                            jint width,
                            jint height);

  void SetBackgroundOpaque(JNIEnv* env, jobject jobj, jboolean opaque);

  void SetupTransitionView(JNIEnv* env, jobject jobj, jstring markup);
  void BeginExitTransition(JNIEnv* env, jobject jobj, jstring css_selector);

  // Notifies the main frame that it can continue navigation (if it was deferred
  // immediately at first response).
  void ResumeResponseDeferredAtStart(JNIEnv* env, jobject obj);

  void SetHasPendingNavigationTransitionForTesting(JNIEnv* env, jobject obj);

  jint GetCurrentRenderProcessId(JNIEnv* env, jobject obj);

  // --------------------------------------------------------------------------
  // Public methods that call to Java via JNI
  // --------------------------------------------------------------------------

  // This method is invoked when the request is deferred immediately after
  // receiving response headers.
  void DidDeferAfterResponseStarted(
      const TransitionLayerData& transition_data);

  // This method is invoked when a navigation transition is detected, to
  // determine if the embedder intends to handle it.
  bool WillHandleDeferAfterResponseStarted();

  // This method is invoked when a navigation transition has started.
  void DidStartNavigationTransitionForFrame(int64 frame_id);

  void OnSmartClipDataExtracted(const base::string16& text,
                                const base::string16& html,
                                const gfx::Rect& clip_rect);

  // Creates a popup menu with |items|.
  // |multiple| defines if it should support multi-select.
  // If not |multiple|, |selected_item| sets the initially selected item.
  // Otherwise, item's "checked" flag selects it.
  void ShowSelectPopupMenu(const gfx::Rect& bounds,
                           const std::vector<MenuItem>& items,
                           int selected_item,
                           bool multiple);
  // Hides a visible popup menu.
  void HideSelectPopupMenu();

  // All sizes and offsets are in CSS pixels as cached by the renderer.
  void UpdateFrameInfo(const gfx::Vector2dF& scroll_offset,
                       float page_scale_factor,
                       const gfx::Vector2dF& page_scale_factor_limits,
                       const gfx::SizeF& content_size,
                       const gfx::SizeF& viewport_size,
                       const gfx::Vector2dF& controls_offset,
                       const gfx::Vector2dF& content_offset,
                       float overdraw_bottom_height);

  void UpdateImeAdapter(long native_ime_adapter,
                        int text_input_type,
                        int text_input_flags,
                        const std::string& text,
                        int selection_start,
                        int selection_end,
                        int composition_start,
                        int composition_end,
                        bool show_ime_if_needed,
                        bool is_non_ime_change);
  void SetTitle(const base::string16& title);
  void OnBackgroundColorChanged(SkColor color);

  bool HasFocus();
  void OnGestureEventAck(const blink::WebGestureEvent& event,
                         InputEventAckState ack_result);
  bool FilterInputEvent(const blink::WebInputEvent& event);
  void OnSelectionChanged(const std::string& text);
  void OnSelectionEvent(SelectionEventType event,
                        const gfx::PointF& anchor_position);
  scoped_ptr<TouchHandleDrawable> CreatePopupTouchHandleDrawable();

  void StartContentIntent(const GURL& content_url);

  // Shows the disambiguation popup
  // |target_rect|   --> window coordinates which |zoomed_bitmap| represents
  // |zoomed_bitmap| --> magnified image of potential touch targets
  void ShowDisambiguationPopup(
      const gfx::Rect& target_rect, const SkBitmap& zoomed_bitmap);

  // Creates a java-side touch event, used for injecting touch event for
  // testing/benchmarking purposes
  base::android::ScopedJavaLocalRef<jobject> CreateTouchEventSynthesizer();

  base::android::ScopedJavaLocalRef<jobject> GetContentVideoViewClient();

  // Returns the context that the ContentViewCore was created with, it would
  // typically be an Activity context for an on screen view.
  base::android::ScopedJavaLocalRef<jobject> GetContext();

  // Returns True if the given media should be blocked to load.
  bool ShouldBlockMediaRequest(const GURL& url);

  void DidStopFlinging();

  // Returns the viewport size after accounting for the viewport offset.
  gfx::Size GetViewSize() const;

  void SetAccessibilityEnabledInternal(bool enabled);

  bool IsFullscreenRequiredForOrientationLock() const;

  // --------------------------------------------------------------------------
  // Methods called from native code
  // --------------------------------------------------------------------------

  gfx::Size GetPhysicalBackingSize() const;
  gfx::Size GetViewportSizeDip() const;
  gfx::Size GetViewportSizeOffsetDip() const;
  float GetOverdrawBottomHeightDip() const;

  void AttachLayer(scoped_refptr<cc::Layer> layer);
  void RemoveLayer(scoped_refptr<cc::Layer> layer);

  void SelectBetweenCoordinates(const gfx::PointF& start,
                                const gfx::PointF& end);

 private:
  class ContentViewUserData;

  friend class ContentViewUserData;
  virtual ~ContentViewCoreImpl();

  // WebContentsObserver implementation.
  virtual void RenderViewReady() OVERRIDE;
  virtual void RenderViewHostChanged(RenderViewHost* old_host,
                                     RenderViewHost* new_host) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

  // --------------------------------------------------------------------------
  // Other private methods and data
  // --------------------------------------------------------------------------

  void InitWebContents();

  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid();

  blink::WebGestureEvent MakeGestureEvent(
      blink::WebInputEvent::Type type, int64 time_ms, float x, float y) const;

  gfx::Size GetViewportSizePix() const;
  gfx::Size GetViewportSizeOffsetPix() const;

  void DeleteScaledSnapshotTexture();

  bool OnMotionEvent(const ui::MotionEvent& event);
  void SendGestureEvent(const blink::WebGestureEvent& event);

  // Update focus state of the RenderWidgetHostView.
  void SetFocusInternal(bool focused);

  // Send device_orientation_ to renderer.
  void SendOrientationChangeEventInternal();

  float dpi_scale() const { return dpi_scale_; }

  // A weak reference to the Java ContentViewCore object.
  JavaObjectWeakGlobalRef java_ref_;

  // Reference to the current WebContents used to determine how and what to
  // display in the ContentViewCore.
  WebContentsImpl* web_contents_;

  // A compositor layer containing any layer that should be shown.
  scoped_refptr<cc::Layer> root_layer_;

  // Device scale factor.
  float dpi_scale_;

  // The Android view that can be used to add and remove decoration layers
  // like AutofillPopup.
  ui::ViewAndroid* view_android_;

  // The owning window that has a hold of main application activity.
  ui::WindowAndroid* window_android_;

  // The cache of device's current orientation set from Java side, this value
  // will be sent to Renderer once it is ready.
  int device_orientation_;

  bool accessibility_enabled_;

  // Manages injecting Java objects.
  scoped_ptr<GinJavaBridgeDispatcherHost>
      java_bridge_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(ContentViewCoreImpl);
};

bool RegisterContentViewCore(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_H_
