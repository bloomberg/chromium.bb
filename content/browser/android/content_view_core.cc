// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_core.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/input/web_input_event_builders_android.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/menu_item.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/common/user_agent.h"
#include "jni/ContentViewCoreImpl_jni.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {

namespace {

// Describes the type and enabled state of a select popup item.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.browser.input
enum PopupItemType {
  // Popup item is of type group
  POPUP_ITEM_TYPE_GROUP,

  // Popup item is disabled
  POPUP_ITEM_TYPE_DISABLED,

  // Popup item is enabled
  POPUP_ITEM_TYPE_ENABLED,
};

const void* const kContentViewUserDataKey = &kContentViewUserDataKey;

int GetRenderProcessIdFromRenderViewHost(RenderViewHost* host) {
  DCHECK(host);
  RenderProcessHost* render_process = host->GetProcess();
  DCHECK(render_process);
  if (render_process->HasConnection())
    return render_process->GetHandle();
  return 0;
}

int ToGestureEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::kGestureScrollBegin:
      return ui::GESTURE_EVENT_TYPE_SCROLL_START;
    case WebInputEvent::kGestureScrollEnd:
      return ui::GESTURE_EVENT_TYPE_SCROLL_END;
    case WebInputEvent::kGestureScrollUpdate:
      return ui::GESTURE_EVENT_TYPE_SCROLL_BY;
    case WebInputEvent::kGestureFlingStart:
      return ui::GESTURE_EVENT_TYPE_FLING_START;
    case WebInputEvent::kGestureFlingCancel:
      return ui::GESTURE_EVENT_TYPE_FLING_CANCEL;
    case WebInputEvent::kGestureShowPress:
      return ui::GESTURE_EVENT_TYPE_SHOW_PRESS;
    case WebInputEvent::kGestureTap:
      return ui::GESTURE_EVENT_TYPE_SINGLE_TAP_CONFIRMED;
    case WebInputEvent::kGestureTapUnconfirmed:
      return ui::GESTURE_EVENT_TYPE_SINGLE_TAP_UNCONFIRMED;
    case WebInputEvent::kGestureTapDown:
      return ui::GESTURE_EVENT_TYPE_TAP_DOWN;
    case WebInputEvent::kGestureTapCancel:
      return ui::GESTURE_EVENT_TYPE_TAP_CANCEL;
    case WebInputEvent::kGestureDoubleTap:
      return ui::GESTURE_EVENT_TYPE_DOUBLE_TAP;
    case WebInputEvent::kGestureLongPress:
      return ui::GESTURE_EVENT_TYPE_LONG_PRESS;
    case WebInputEvent::kGestureLongTap:
      return ui::GESTURE_EVENT_TYPE_LONG_TAP;
    case WebInputEvent::kGesturePinchBegin:
      return ui::GESTURE_EVENT_TYPE_PINCH_BEGIN;
    case WebInputEvent::kGesturePinchEnd:
      return ui::GESTURE_EVENT_TYPE_PINCH_END;
    case WebInputEvent::kGesturePinchUpdate:
      return ui::GESTURE_EVENT_TYPE_PINCH_BY;
    case WebInputEvent::kGestureTwoFingerTap:
    default:
      NOTREACHED() << "Invalid source gesture type: "
                   << WebInputEvent::GetName(type);
      return -1;
  }
}

}  // namespace

// Enables a callback when the underlying WebContents is destroyed, to enable
// nulling the back-pointer.
class ContentViewCore::ContentViewUserData
    : public base::SupportsUserData::Data {
 public:
  explicit ContentViewUserData(ContentViewCore* content_view_core)
      : content_view_core_(content_view_core) {}

  ~ContentViewUserData() override {
    // TODO(joth): When chrome has finished removing the TabContents class (see
    // crbug.com/107201) consider inverting relationship, so ContentViewCore
    // would own WebContents. That effectively implies making the WebContents
    // destructor private on Android.
    delete content_view_core_;
  }

  ContentViewCore* get() const { return content_view_core_; }

 private:
  // Not using scoped_ptr as ContentViewCore destructor is private.
  ContentViewCore* content_view_core_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentViewUserData);
};

// static
ContentViewCore* ContentViewCore::FromWebContents(
    content::WebContents* web_contents) {
  ContentViewCore::ContentViewUserData* data =
      static_cast<ContentViewCore::ContentViewUserData*>(
          web_contents->GetUserData(kContentViewUserDataKey));
  return data ? data->get() : NULL;
}

ContentViewCore::ContentViewCore(JNIEnv* env,
                                 const JavaRef<jobject>& obj,
                                 WebContents* web_contents,
                                 float dpi_scale)
    : WebContentsObserver(web_contents),
      java_ref_(env, obj),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)),
      dpi_scale_(dpi_scale),
      device_orientation_(0) {
  GetViewAndroid()->SetLayer(cc::Layer::Create());

  // Currently, the only use case we have for overriding a user agent involves
  // spoofing a desktop Linux user agent for "Request desktop site".
  // Automatically set it for all WebContents so that it is available when a
  // NavigationEntry requires the user agent to be overridden.
  const char kLinuxInfoStr[] = "X11; Linux x86_64";
  std::string product = content::GetContentClient()->GetProduct();
  std::string spoofed_ua =
      BuildUserAgentFromOSAndProduct(kLinuxInfoStr, product);
  web_contents->SetUserAgentOverride(spoofed_ua);

  InitWebContents();
}

ContentViewCore::~ContentViewCore() {
  for (auto* host : web_contents_->GetAllRenderWidgetHosts()) {
    static_cast<RenderWidgetHostViewAndroid*>(host->GetView())
        ->OnContentViewCoreDestroyed();
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  java_ref_.reset();
  if (!j_obj.is_null()) {
    Java_ContentViewCoreImpl_onNativeContentViewCoreDestroyed(
        env, j_obj, reinterpret_cast<intptr_t>(this));
  }
}

void ContentViewCore::UpdateWindowAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jwindow_android) {
  ui::WindowAndroid* window =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  auto* old_window = GetWindowAndroid();
  if (window == old_window)
    return;

  auto* view = GetViewAndroid();
  if (old_window)
    view->RemoveFromParent();
  if (window)
    window->AddChild(view);
}

base::android::ScopedJavaLocalRef<jobject>
ContentViewCore::GetWebContentsAndroid(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  return web_contents_->GetJavaWebContents();
}

base::android::ScopedJavaLocalRef<jobject>
ContentViewCore::GetJavaWindowAndroid(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  if (!GetWindowAndroid())
    return ScopedJavaLocalRef<jobject>();
  return GetWindowAndroid()->GetJavaObject();
}

void ContentViewCore::OnJavaContentViewCoreDestroyed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK(env->IsSameObject(java_ref_.get(env).obj(), obj));
  java_ref_.reset();
  // Java peer has gone, ContentViewCore is not functional and waits to
  // be destroyed with WebContents.
  // We need to reset WebContentsViewAndroid's reference, otherwise, there
  // could have call in when swapping the WebContents,
  // see http://crbug.com/383939 .
  DCHECK(web_contents_);
  static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(web_contents_)->GetView())
      ->SetContentViewCore(NULL);
}

void ContentViewCore::InitWebContents() {
  DCHECK(web_contents_);
  static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(web_contents_)->GetView())
      ->SetContentViewCore(this);
  DCHECK(!web_contents_->GetUserData(kContentViewUserDataKey));
  web_contents_->SetUserData(kContentViewUserDataKey,
                             std::make_unique<ContentViewUserData>(this));
}

void ContentViewCore::RenderViewReady() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCoreImpl_onRenderProcessChange(env, obj);

  if (device_orientation_ != 0)
    SendOrientationChangeEventInternal();
}

void ContentViewCore::RenderViewHostChanged(RenderViewHost* old_host,
                                            RenderViewHost* new_host) {
  int old_pid = 0;
  if (old_host) {
    old_pid = GetRenderProcessIdFromRenderViewHost(old_host);

    RenderWidgetHostViewAndroid* view =
        static_cast<RenderWidgetHostViewAndroid*>(
            old_host->GetWidget()->GetView());
    if (view)
      view->SetContentViewCore(NULL);

    view = static_cast<RenderWidgetHostViewAndroid*>(
        new_host->GetWidget()->GetView());
    if (view)
      view->SetContentViewCore(this);
  }
  int new_pid =
      GetRenderProcessIdFromRenderViewHost(web_contents_->GetRenderViewHost());
  if (new_pid != old_pid) {
    // Notify the Java side that the renderer process changed.
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (!obj.is_null()) {
      Java_ContentViewCoreImpl_onRenderProcessChange(env, obj);
    }
  }

  SetFocusInternal(GetViewAndroid()->HasFocus());
}

RenderWidgetHostViewAndroid* ContentViewCore::GetRenderWidgetHostViewAndroid()
    const {
  RenderWidgetHostView* rwhv = NULL;
  if (web_contents_) {
    rwhv = web_contents_->GetRenderWidgetHostView();
    if (web_contents_->ShowingInterstitialPage()) {
      rwhv = web_contents_->GetInterstitialPage()
                 ->GetMainFrame()
                 ->GetRenderViewHost()
                 ->GetWidget()
                 ->GetView();
    }
  }
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

ScopedJavaLocalRef<jobject> ContentViewCore::GetJavaObject() {
  JNIEnv* env = AttachCurrentThread();
  return java_ref_.get(env);
}

jint ContentViewCore::GetBackgroundColor(JNIEnv* env, jobject obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return SK_ColorWHITE;
  return rwhva->GetCachedBackgroundColor();
}

// All positions and sizes (except |top_shown_pix|) are in CSS pixels.
// Note that viewport_width/height is a best effort based.
// ContentViewCore has the actual information about the physical viewport size.
void ContentViewCore::UpdateFrameInfo(const gfx::Vector2dF& scroll_offset,
                                      float page_scale_factor,
                                      const float min_page_scale,
                                      const float max_page_scale,
                                      const gfx::SizeF& content_size,
                                      const gfx::SizeF& viewport_size,
                                      const float content_offset,
                                      const float top_shown_pix,
                                      bool top_changed,
                                      bool is_mobile_optimized_hint) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null() || !GetWindowAndroid())
    return;

  GetViewAndroid()->UpdateFrameInfo({viewport_size, content_offset});

  // Current viewport size in css.
  gfx::SizeF view_size = gfx::SizeF(gfx::ScaleToCeiledSize(
      GetViewportSizePix(), 1.0f / (dpi_scale() * page_scale_factor)));

  // Adjust content size to be always at least as big as the actual
  // viewport (as set by onSizeChanged).
  float content_width = std::max(content_size.width(), view_size.width());
  float content_height = std::max(content_size.height(), view_size.height());

  Java_ContentViewCoreImpl_updateFrameInfo(
      env, obj, scroll_offset.x(), scroll_offset.y(), page_scale_factor,
      min_page_scale, max_page_scale, content_width, content_height,
      viewport_size.width(), viewport_size.height(), top_shown_pix, top_changed,
      is_mobile_optimized_hint);
}

void ContentViewCore::ShowSelectPopupMenu(RenderFrameHost* frame,
                                          const gfx::Rect& bounds,
                                          const std::vector<MenuItem>& items,
                                          int selected_item,
                                          bool multiple,
                                          bool right_aligned) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;

  // For multi-select list popups we find the list of previous selections by
  // iterating through the items. But for single selection popups we take the
  // given |selected_item| as is.
  ScopedJavaLocalRef<jintArray> selected_array;
  if (multiple) {
    std::unique_ptr<jint[]> native_selected_array(new jint[items.size()]);
    size_t selected_count = 0;
    for (size_t i = 0; i < items.size(); ++i) {
      if (items[i].checked)
        native_selected_array[selected_count++] = i;
    }

    selected_array =
        ScopedJavaLocalRef<jintArray>(env, env->NewIntArray(selected_count));
    env->SetIntArrayRegion(selected_array.obj(), 0, selected_count,
                           native_selected_array.get());
  } else {
    selected_array = ScopedJavaLocalRef<jintArray>(env, env->NewIntArray(1));
    jint value = selected_item;
    env->SetIntArrayRegion(selected_array.obj(), 0, 1, &value);
  }

  ScopedJavaLocalRef<jintArray> enabled_array(env,
                                              env->NewIntArray(items.size()));
  std::vector<base::string16> labels;
  labels.reserve(items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    labels.push_back(items[i].label);
    jint enabled = (items[i].type == MenuItem::GROUP
                        ? POPUP_ITEM_TYPE_GROUP
                        : (items[i].enabled ? POPUP_ITEM_TYPE_ENABLED
                                            : POPUP_ITEM_TYPE_DISABLED));
    env->SetIntArrayRegion(enabled_array.obj(), i, 1, &enabled);
  }
  ScopedJavaLocalRef<jobjectArray> items_array(
      base::android::ToJavaArrayOfStrings(env, labels));
  ui::ViewAndroid* view = GetViewAndroid();
  select_popup_ = view->AcquireAnchorView();
  const ScopedJavaLocalRef<jobject> popup_view = select_popup_.view();
  if (popup_view.is_null())
    return;
  // |bounds| is in physical pixels if --use-zoom-for-dsf is enabled. Otherwise,
  // it is in DIP pixels.
  gfx::RectF bounds_dip = gfx::RectF(bounds);
  if (IsUseZoomForDSFEnabled())
    bounds_dip.Scale(1 / dpi_scale_);
  view->SetAnchorRect(popup_view, bounds_dip);
  Java_ContentViewCoreImpl_showSelectPopup(
      env, j_obj, popup_view, reinterpret_cast<intptr_t>(frame), items_array,
      enabled_array, multiple, selected_array, right_aligned);
}

void ContentViewCore::HideSelectPopupMenu() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null())
    Java_ContentViewCoreImpl_hideSelectPopup(env, j_obj);
  select_popup_.Reset();
}

bool ContentViewCore::FilterInputEvent(const blink::WebInputEvent& event) {
  if (event.GetType() != WebInputEvent::kGestureTap &&
      event.GetType() != WebInputEvent::kGestureLongTap &&
      event.GetType() != WebInputEvent::kGestureLongPress &&
      event.GetType() != WebInputEvent::kMouseDown)
    return false;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return false;

  GetViewAndroid()->RequestFocus();

  if (event.GetType() == WebInputEvent::kMouseDown)
    return false;

  const blink::WebGestureEvent& gesture =
      static_cast<const blink::WebGestureEvent&>(event);
  int gesture_type = ToGestureEventType(event.GetType());
  return Java_ContentViewCoreImpl_filterTapOrPressEvent(
      env, j_obj, gesture_type, gesture.x * dpi_scale(),
      gesture.y * dpi_scale());
}

void ContentViewCore::RequestDisallowInterceptTouchEvent() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCoreImpl_requestDisallowInterceptTouchEvent(env, obj);
}

gfx::Size ContentViewCore::GetViewportSizePix() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return gfx::Size();
  return gfx::Size(Java_ContentViewCoreImpl_getViewportWidthPix(env, j_obj),
                   Java_ContentViewCoreImpl_getViewportHeightPix(env, j_obj));
}

int ContentViewCore::GetMouseWheelMinimumGranularity() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return 0;
  return Java_ContentViewCoreImpl_getMouseWheelTickMultiplier(env, j_obj);
}

void ContentViewCore::SendScreenRectsAndResizeWidget() {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (view) {
    // |SendScreenRects()| indirectly calls GetViewSize() that asks Java layer.
    web_contents_->SendScreenRects();
    view->WasResized();
  }
}

ui::WindowAndroid* ContentViewCore::GetWindowAndroid() const {
  return GetViewAndroid()->GetWindowAndroid();
}

ui::ViewAndroid* ContentViewCore::GetViewAndroid() const {
  return web_contents_->GetView()->GetNativeView();
}

// ----------------------------------------------------------------------------
// Methods called from Java via JNI
// ----------------------------------------------------------------------------

void ContentViewCore::SelectPopupMenuItems(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong selectPopupSourceFrame,
    const JavaParamRef<jintArray>& indices) {
  RenderFrameHostImpl* rfhi =
      reinterpret_cast<RenderFrameHostImpl*>(selectPopupSourceFrame);
  DCHECK(rfhi);
  if (indices == NULL) {
    rfhi->DidCancelPopupMenu();
    return;
  }

  int selected_count = env->GetArrayLength(indices);
  std::vector<int> selected_indices;
  jint* indices_ptr = env->GetIntArrayElements(indices, NULL);
  for (int i = 0; i < selected_count; ++i)
    selected_indices.push_back(indices_ptr[i]);
  env->ReleaseIntArrayElements(indices, indices_ptr, JNI_ABORT);
  rfhi->DidSelectPopupMenuItems(selected_indices);
}

WebContents* ContentViewCore::GetWebContents() const {
  return web_contents_;
}

void ContentViewCore::SetFocus(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               jboolean focused) {
  SetFocusInternal(focused);
}

void ContentViewCore::SetDIPScale(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jfloat dpi_scale) {
  if (dpi_scale_ == dpi_scale)
    return;

  dpi_scale_ = dpi_scale;
  SendScreenRectsAndResizeWidget();
}

void ContentViewCore::SetFocusInternal(bool focused) {
  if (!GetRenderWidgetHostViewAndroid())
    return;

  if (focused)
    GetRenderWidgetHostViewAndroid()->GotFocus();
  else
    GetRenderWidgetHostViewAndroid()->LostFocus();
}

int ContentViewCore::GetTopControlsShrinkBlinkHeightPixForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  return !rwhv || !rwhv->DoBrowserControlsShrinkBlinkSize()
             ? 0
             : rwhv->GetTopControlsHeight() * dpi_scale_;
}

void ContentViewCore::SendOrientationChangeEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint orientation) {
  if (device_orientation_ != orientation) {
    base::RecordAction(base::UserMetricsAction("ScreenOrientationChange"));
    device_orientation_ = orientation;
    SendOrientationChangeEventInternal();
  }
}

WebGestureEvent ContentViewCore::MakeGestureEvent(WebInputEvent::Type type,
                                                  int64_t time_ms,
                                                  float x,
                                                  float y) const {
  return WebGestureEventBuilder::Build(type, time_ms / 1000.0, x / dpi_scale(),
                                       y / dpi_scale());
}

void ContentViewCore::SendGestureEvent(const blink::WebGestureEvent& event) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SendGestureEvent(event);
}

void ContentViewCore::ScrollBegin(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jlong time_ms,
                                  jfloat x,
                                  jfloat y,
                                  jfloat hintx,
                                  jfloat hinty,
                                  jboolean target_viewport,
                                  jboolean from_gamepad) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::kGestureScrollBegin, time_ms, x, y);
  event.data.scroll_begin.delta_x_hint = hintx / dpi_scale();
  event.data.scroll_begin.delta_y_hint = hinty / dpi_scale();
  event.data.scroll_begin.target_viewport = target_viewport;

  if (from_gamepad)
    event.source_device = blink::kWebGestureDeviceSyntheticAutoscroll;

  SendGestureEvent(event);
}

void ContentViewCore::ScrollEnd(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                jlong time_ms) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::kGestureScrollEnd, time_ms, 0, 0);
  SendGestureEvent(event);
}

void ContentViewCore::ScrollBy(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               jlong time_ms,
                               jfloat x,
                               jfloat y,
                               jfloat dx,
                               jfloat dy) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::kGestureScrollUpdate, time_ms, x, y);
  event.data.scroll_update.delta_x = -dx / dpi_scale();
  event.data.scroll_update.delta_y = -dy / dpi_scale();

  SendGestureEvent(event);
}

void ContentViewCore::DoubleTap(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                jlong time_ms,
                                jfloat x,
                                jfloat y) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::kGestureDoubleTap, time_ms, x, y);
  // Set the tap count to 1 even for DoubleTap, in order to be consistent with
  // double tap behavior on a mobile viewport. See crbug.com/234986 for context.
  event.data.tap.tap_count = 1;

  SendGestureEvent(event);
}

void ContentViewCore::SetTextHandlesTemporarilyHidden(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean hidden) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SetTextHandlesTemporarilyHidden(hidden);
}

void ContentViewCore::ResetGestureDetection(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->ResetGestureDetection();
}

void ContentViewCore::SetDoubleTapSupportEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SetDoubleTapSupportEnabled(enabled);
}

void ContentViewCore::SetMultiTouchZoomSupportEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SetMultiTouchZoomSupportEnabled(enabled);
}

void ContentViewCore::OnTouchDown(
    const base::android::ScopedJavaLocalRef<jobject>& event) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCoreImpl_onTouchDown(env, obj, event);
}

void ContentViewCore::SetTextTrackSettings(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean textTracksEnabled,
    const JavaParamRef<jstring>& textTrackBackgroundColor,
    const JavaParamRef<jstring>& textTrackFontFamily,
    const JavaParamRef<jstring>& textTrackFontStyle,
    const JavaParamRef<jstring>& textTrackFontVariant,
    const JavaParamRef<jstring>& textTrackTextColor,
    const JavaParamRef<jstring>& textTrackTextShadow,
    const JavaParamRef<jstring>& textTrackTextSize) {
  FrameMsg_TextTrackSettings_Params params;
  params.text_tracks_enabled = textTracksEnabled;
  params.text_track_background_color =
      ConvertJavaStringToUTF8(env, textTrackBackgroundColor);
  params.text_track_font_family =
      ConvertJavaStringToUTF8(env, textTrackFontFamily);
  params.text_track_font_style =
      ConvertJavaStringToUTF8(env, textTrackFontStyle);
  params.text_track_font_variant =
      ConvertJavaStringToUTF8(env, textTrackFontVariant);
  params.text_track_text_color =
      ConvertJavaStringToUTF8(env, textTrackTextColor);
  params.text_track_text_shadow =
      ConvertJavaStringToUTF8(env, textTrackTextShadow);
  params.text_track_text_size = ConvertJavaStringToUTF8(env, textTrackTextSize);
  web_contents_->GetMainFrame()->SetTextTrackSettings(params);
}

void ContentViewCore::SendOrientationChangeEventInternal() {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->UpdateScreenInfo(GetViewAndroid());

  static_cast<WebContentsImpl*>(web_contents())->OnScreenOrientationChange();
}

jboolean ContentViewCore::UsingSynchronousCompositing(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return content::GetContentClient()->UsingSynchronousCompositing();
}

void ContentViewCore::HidePopupsAndPreserveSelection() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_ContentViewCoreImpl_hidePopupsAndPreserveSelection(env, obj);
}

void ContentViewCore::WebContentsDestroyed() {
  WebContentsViewAndroid* wcva = static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(web_contents())->GetView());
  DCHECK(wcva);
  wcva->SetContentViewCore(NULL);
}

// This is called for each ContentView.
jlong JNI_ContentViewCoreImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jview_android_delegate,
    const JavaParamRef<jobject>& jwindow_android,
    jfloat dip_scale) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromJavaWebContents(jweb_contents));
  CHECK(web_contents)
      << "A ContentViewCore should be created with a valid WebContents.";
  ui::ViewAndroid* view_android = web_contents->GetView()->GetNativeView();
  view_android->SetDelegate(jview_android_delegate);

  ui::WindowAndroid* window_android =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  DCHECK(window_android);
  window_android->AddChild(view_android);

  ContentViewCore* view =
      new ContentViewCore(env, obj, web_contents, dip_scale);
  return reinterpret_cast<intptr_t>(view);
}

static ScopedJavaLocalRef<jobject>
JNI_ContentViewCoreImpl_FromWebContentsAndroid(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return ScopedJavaLocalRef<jobject>();

  ContentViewCore* view = ContentViewCore::FromWebContents(web_contents);
  if (!view)
    return ScopedJavaLocalRef<jobject>();

  return view->GetJavaObject();
}

}  // namespace content
