// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_core_impl.h"

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
#include "cc/output/begin_frame_args.h"
#include "content/browser/android/gesture_event_type.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"
#include "content/browser/android/load_url_params.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/input/web_input_event_builders_android.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
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
#include "content/public/common/user_agent.h"
#include "device/geolocation/geolocation_service_context.h"
#include "jni/ContentViewCore_jni.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
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

ScopedJavaLocalRef<jobject> CreateJavaRect(
    JNIEnv* env,
    const gfx::Rect& rect) {
  return ScopedJavaLocalRef<jobject>(
      Java_ContentViewCore_createRect(env,
                                      static_cast<int>(rect.x()),
                                      static_cast<int>(rect.y()),
                                      static_cast<int>(rect.right()),
                                      static_cast<int>(rect.bottom())));
}

int ToGestureEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::kGestureScrollBegin:
      return GESTURE_EVENT_TYPE_SCROLL_START;
    case WebInputEvent::kGestureScrollEnd:
      return GESTURE_EVENT_TYPE_SCROLL_END;
    case WebInputEvent::kGestureScrollUpdate:
      return GESTURE_EVENT_TYPE_SCROLL_BY;
    case WebInputEvent::kGestureFlingStart:
      return GESTURE_EVENT_TYPE_FLING_START;
    case WebInputEvent::kGestureFlingCancel:
      return GESTURE_EVENT_TYPE_FLING_CANCEL;
    case WebInputEvent::kGestureShowPress:
      return GESTURE_EVENT_TYPE_SHOW_PRESS;
    case WebInputEvent::kGestureTap:
      return GESTURE_EVENT_TYPE_SINGLE_TAP_CONFIRMED;
    case WebInputEvent::kGestureTapUnconfirmed:
      return GESTURE_EVENT_TYPE_SINGLE_TAP_UNCONFIRMED;
    case WebInputEvent::kGestureTapDown:
      return GESTURE_EVENT_TYPE_TAP_DOWN;
    case WebInputEvent::kGestureTapCancel:
      return GESTURE_EVENT_TYPE_TAP_CANCEL;
    case WebInputEvent::kGestureDoubleTap:
      return GESTURE_EVENT_TYPE_DOUBLE_TAP;
    case WebInputEvent::kGestureLongPress:
      return GESTURE_EVENT_TYPE_LONG_PRESS;
    case WebInputEvent::kGestureLongTap:
      return GESTURE_EVENT_TYPE_LONG_TAP;
    case WebInputEvent::kGesturePinchBegin:
      return GESTURE_EVENT_TYPE_PINCH_BEGIN;
    case WebInputEvent::kGesturePinchEnd:
      return GESTURE_EVENT_TYPE_PINCH_END;
    case WebInputEvent::kGesturePinchUpdate:
      return GESTURE_EVENT_TYPE_PINCH_BY;
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
class ContentViewCoreImpl::ContentViewUserData
    : public base::SupportsUserData::Data {
 public:
  explicit ContentViewUserData(ContentViewCoreImpl* content_view_core)
      : content_view_core_(content_view_core) {
  }

  ~ContentViewUserData() override {
    // TODO(joth): When chrome has finished removing the TabContents class (see
    // crbug.com/107201) consider inverting relationship, so ContentViewCore
    // would own WebContents. That effectively implies making the WebContents
    // destructor private on Android.
    delete content_view_core_;
  }

  ContentViewCoreImpl* get() const { return content_view_core_; }

 private:
  // Not using scoped_ptr as ContentViewCoreImpl destructor is private.
  ContentViewCoreImpl* content_view_core_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentViewUserData);
};

// static
ContentViewCoreImpl* ContentViewCoreImpl::FromWebContents(
    content::WebContents* web_contents) {
  ContentViewCoreImpl::ContentViewUserData* data =
      static_cast<ContentViewCoreImpl::ContentViewUserData*>(
          web_contents->GetUserData(kContentViewUserDataKey));
  return data ? data->get() : NULL;
}

// static
ContentViewCore* ContentViewCore::FromWebContents(
    content::WebContents* web_contents) {
  return ContentViewCoreImpl::FromWebContents(web_contents);
}

ContentViewCoreImpl::ContentViewCoreImpl(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    WebContents* web_contents,
    float dpi_scale,
    const JavaRef<jobject>& java_bridge_retained_object_set)
    : WebContentsObserver(web_contents),
      java_ref_(env, obj),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)),
      page_scale_(1),
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

  java_bridge_dispatcher_host_ =
      new GinJavaBridgeDispatcherHost(web_contents,
                                      java_bridge_retained_object_set);

  InitWebContents();
}

void ContentViewCoreImpl::AddObserver(
    ContentViewCoreImplObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ContentViewCoreImpl::RemoveObserver(
    ContentViewCoreImplObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

ContentViewCoreImpl::~ContentViewCoreImpl() {
  for (auto& observer : observer_list_)
    observer.OnContentViewCoreDestroyed();
  observer_list_.Clear();

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  java_ref_.reset();
  if (!j_obj.is_null()) {
    Java_ContentViewCore_onNativeContentViewCoreDestroyed(
        env, j_obj, reinterpret_cast<intptr_t>(this));
  }
}

void ContentViewCoreImpl::UpdateWindowAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong window_android) {
  ui::ViewAndroid* view = GetViewAndroid();
  ui::WindowAndroid* window =
      reinterpret_cast<ui::WindowAndroid*>(window_android);
  if (window == GetWindowAndroid())
    return;
  if (GetWindowAndroid()) {
    for (auto& observer : observer_list_)
      observer.OnDetachedFromWindow();
    view->RemoveFromParent();
  }
  if (window) {
    window->AddChild(view);
    for (auto& observer : observer_list_)
      observer.OnAttachedToWindow();
  }
}

base::android::ScopedJavaLocalRef<jobject>
ContentViewCoreImpl::GetWebContentsAndroid(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  return web_contents_->GetJavaWebContents();
}

base::android::ScopedJavaLocalRef<jobject>
ContentViewCoreImpl::GetJavaWindowAndroid(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  if (!GetWindowAndroid())
    return ScopedJavaLocalRef<jobject>();
  return GetWindowAndroid()->GetJavaObject();
}

void ContentViewCoreImpl::OnJavaContentViewCoreDestroyed(
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
      static_cast<WebContentsImpl*>(web_contents_)->GetView())->
          SetContentViewCore(NULL);
}

void ContentViewCoreImpl::InitWebContents() {
  DCHECK(web_contents_);
  static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(web_contents_)->GetView())->
          SetContentViewCore(this);
  DCHECK(!web_contents_->GetUserData(kContentViewUserDataKey));
  web_contents_->SetUserData(kContentViewUserDataKey,
                             base::MakeUnique<ContentViewUserData>(this));
}

void ContentViewCoreImpl::RenderViewReady() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_onRenderProcessChange(env, obj);

  if (device_orientation_ != 0)
    SendOrientationChangeEventInternal();
}

void ContentViewCoreImpl::RenderViewHostChanged(RenderViewHost* old_host,
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
  int new_pid = GetRenderProcessIdFromRenderViewHost(
      web_contents_->GetRenderViewHost());
  if (new_pid != old_pid) {
    // Notify the Java side that the renderer process changed.
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (!obj.is_null()) {
      Java_ContentViewCore_onRenderProcessChange(env, obj);
    }
  }

  SetFocusInternal(HasFocus());
}

RenderWidgetHostViewAndroid*
    ContentViewCoreImpl::GetRenderWidgetHostViewAndroid() const {
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

ScopedJavaLocalRef<jobject> ContentViewCoreImpl::GetJavaObject() {
  JNIEnv* env = AttachCurrentThread();
  return java_ref_.get(env);
}

jint ContentViewCoreImpl::GetBackgroundColor(JNIEnv* env, jobject obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return SK_ColorWHITE;
  return rwhva->GetCachedBackgroundColor();
}

// All positions and sizes (except |top_shown_pix|) are in CSS pixels.
// Note that viewport_width/height is a best effort based.
// ContentViewCore has the actual information about the physical viewport size.
void ContentViewCoreImpl::UpdateFrameInfo(
    const gfx::Vector2dF& scroll_offset,
    float page_scale_factor,
    const gfx::Vector2dF& page_scale_factor_limits,
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

  GetViewAndroid()->set_content_offset(gfx::Vector2dF(0.0f, content_offset));

  page_scale_ = page_scale_factor;

  Java_ContentViewCore_updateFrameInfo(
      env, obj, scroll_offset.x(), scroll_offset.y(), page_scale_factor,
      page_scale_factor_limits.x(), page_scale_factor_limits.y(),
      content_size.width(), content_size.height(), viewport_size.width(),
      viewport_size.height(), top_shown_pix, top_changed,
      is_mobile_optimized_hint);
}

void ContentViewCoreImpl::ShowSelectPopupMenu(
    RenderFrameHost* frame,
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

    selected_array = ScopedJavaLocalRef<jintArray>(
        env, env->NewIntArray(selected_count));
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
    jint enabled =
        (items[i].type == MenuItem::GROUP ? POPUP_ITEM_TYPE_GROUP :
            (items[i].enabled ? POPUP_ITEM_TYPE_ENABLED :
                POPUP_ITEM_TYPE_DISABLED));
    env->SetIntArrayRegion(enabled_array.obj(), i, 1, &enabled);
  }
  ScopedJavaLocalRef<jobjectArray> items_array(
      base::android::ToJavaArrayOfStrings(env, labels));
  ui::ViewAndroid* view = GetViewAndroid();
  select_popup_ = view->AcquireAnchorView();
  const ScopedJavaLocalRef<jobject> popup_view = select_popup_.view();
  if (popup_view.is_null())
    return;
  view->SetAnchorRect(popup_view, gfx::RectF(bounds));
  Java_ContentViewCore_showSelectPopup(
      env, j_obj, popup_view, reinterpret_cast<intptr_t>(frame), items_array,
      enabled_array, multiple, selected_array, right_aligned);
}

void ContentViewCoreImpl::HideSelectPopupMenu() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null())
    Java_ContentViewCore_hideSelectPopup(env, j_obj);
  select_popup_.Reset();
}

void ContentViewCoreImpl::OnGestureEventAck(const blink::WebGestureEvent& event,
                                            InputEventAckState ack_result) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;

  switch (event.GetType()) {
    case WebInputEvent::kGestureFlingStart:
      if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED) {
        // The view expects the fling velocity in pixels/s.
        Java_ContentViewCore_onFlingStartEventConsumed(env, j_obj);
      } else {
        // If a scroll ends with a fling, a SCROLL_END event is never sent.
        // However, if that fling went unconsumed, we still need to let the
        // listeners know that scrolling has ended.
        Java_ContentViewCore_onScrollEndEventAck(env, j_obj);
      }
      break;
    case WebInputEvent::kGestureFlingCancel:
      Java_ContentViewCore_onFlingCancelEventAck(env, j_obj);
      break;
    case WebInputEvent::kGestureScrollBegin:
      Java_ContentViewCore_onScrollBeginEventAck(env, j_obj);
      break;
    case WebInputEvent::kGestureScrollUpdate:
      if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
        Java_ContentViewCore_onScrollUpdateGestureConsumed(env, j_obj);
      break;
    case WebInputEvent::kGestureScrollEnd:
      Java_ContentViewCore_onScrollEndEventAck(env, j_obj);
      break;
    case WebInputEvent::kGesturePinchBegin:
      Java_ContentViewCore_onPinchBeginEventAck(env, j_obj);
      break;
    case WebInputEvent::kGesturePinchEnd:
      Java_ContentViewCore_onPinchEndEventAck(env, j_obj);
      break;
    case WebInputEvent::kGestureTap:
      Java_ContentViewCore_onSingleTapEventAck(
          env, j_obj, ack_result == INPUT_EVENT_ACK_STATE_CONSUMED);
      break;
    case WebInputEvent::kGestureLongPress:
      if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
        Java_ContentViewCore_performLongPressHapticFeedback(env, j_obj);
      break;
    default:
      break;
  }
}

bool ContentViewCoreImpl::FilterInputEvent(const blink::WebInputEvent& event) {
  if (event.GetType() != WebInputEvent::kGestureTap &&
      event.GetType() != WebInputEvent::kGestureLongTap &&
      event.GetType() != WebInputEvent::kGestureLongPress &&
      event.GetType() != WebInputEvent::kMouseDown)
    return false;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return false;

  Java_ContentViewCore_requestFocus(env, j_obj);

  if (event.GetType() == WebInputEvent::kMouseDown)
    return false;

  const blink::WebGestureEvent& gesture =
      static_cast<const blink::WebGestureEvent&>(event);
  int gesture_type = ToGestureEventType(event.GetType());
  return Java_ContentViewCore_filterTapOrPressEvent(env, j_obj, gesture_type,
                                                    gesture.x * dpi_scale(),
                                                    gesture.y * dpi_scale());
}

bool ContentViewCoreImpl::HasFocus() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_ContentViewCore_hasFocus(env, obj);
}

void ContentViewCoreImpl::RequestDisallowInterceptTouchEvent() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_requestDisallowInterceptTouchEvent(env, obj);
}

void ContentViewCoreImpl::ShowDisambiguationPopup(
    const gfx::Rect& rect_pixels,
    const SkBitmap& zoomed_bitmap) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jobject> rect_object(CreateJavaRect(env, rect_pixels));

  ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(&zoomed_bitmap);
  DCHECK(!java_bitmap.is_null());

  Java_ContentViewCore_showDisambiguationPopup(env, obj, rect_object,
                                               java_bitmap);
}

ScopedJavaLocalRef<jobject>
ContentViewCoreImpl::CreateMotionEventSynthesizer() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return ScopedJavaLocalRef<jobject>();
  return Java_ContentViewCore_createMotionEventSynthesizer(env, obj);
}

void ContentViewCoreImpl::DidStopFlinging() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_onNativeFlingStopped(env, obj);
}

ScopedJavaLocalRef<jobject> ContentViewCoreImpl::GetContext() const {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return ScopedJavaLocalRef<jobject>();

  return Java_ContentViewCore_getContext(env, obj);
}

gfx::Size ContentViewCoreImpl::GetViewSize() const {
  gfx::Size size = GetViewportSizeDip();
  if (DoBrowserControlsShrinkBlinkSize())
    size.Enlarge(0, -GetTopControlsHeightDip() - GetBottomControlsHeightDip());
  return size;
}

gfx::Size ContentViewCoreImpl::GetViewportSizePix() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return gfx::Size();
  return gfx::Size(Java_ContentViewCore_getViewportWidthPix(env, j_obj),
                   Java_ContentViewCore_getViewportHeightPix(env, j_obj));
}

int ContentViewCoreImpl::GetTopControlsHeightPix() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return 0;
  return Java_ContentViewCore_getTopControlsHeightPix(env, j_obj);
}

int ContentViewCoreImpl::GetBottomControlsHeightPix() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return 0;
  return Java_ContentViewCore_getBottomControlsHeightPix(env, j_obj);
}

gfx::Size ContentViewCoreImpl::GetViewportSizeDip() const {
  return gfx::ScaleToCeiledSize(GetViewportSizePix(), 1.0f / dpi_scale());
}

bool ContentViewCoreImpl::DoBrowserControlsShrinkBlinkSize() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return false;
  return Java_ContentViewCore_doBrowserControlsShrinkBlinkSize(env, j_obj);
}

float ContentViewCoreImpl::GetTopControlsHeightDip() const {
  return GetTopControlsHeightPix() / dpi_scale();
}

float ContentViewCoreImpl::GetBottomControlsHeightDip() const {
  return GetBottomControlsHeightPix() / dpi_scale();
}

void ContentViewCoreImpl::SendScreenRectsAndResizeWidget() {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (view) {
    // |SendScreenRects()| indirectly calls GetViewSize() that asks Java layer.
    web_contents_->SendScreenRects();
    view->WasResized();
  }
}

void ContentViewCoreImpl::MoveRangeSelectionExtent(const gfx::PointF& extent) {
  if (!web_contents_)
    return;

  web_contents_->MoveRangeSelectionExtent(gfx::ToRoundedPoint(extent));
}

void ContentViewCoreImpl::SelectBetweenCoordinates(const gfx::PointF& base,
                                                   const gfx::PointF& extent) {
  if (!web_contents_)
    return;

  gfx::Point base_point = gfx::ToRoundedPoint(base);
  gfx::Point extent_point = gfx::ToRoundedPoint(extent);
  if (base_point == extent_point)
    return;

  web_contents_->SelectRange(base_point, extent_point);
}

ui::WindowAndroid* ContentViewCoreImpl::GetWindowAndroid() const {
  return GetViewAndroid()->GetWindowAndroid();
}

ui::ViewAndroid* ContentViewCoreImpl::GetViewAndroid() const {
  return web_contents_->GetView()->GetNativeView();
}


// ----------------------------------------------------------------------------
// Methods called from Java via JNI
// ----------------------------------------------------------------------------

void ContentViewCoreImpl::SelectPopupMenuItems(
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

WebContents* ContentViewCoreImpl::GetWebContents() const {
  return web_contents_;
}

void ContentViewCoreImpl::SetFocus(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jboolean focused) {
  SetFocusInternal(focused);
}

void ContentViewCoreImpl::SetDIPScale(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jfloat dpi_scale) {
  if (dpi_scale_ == dpi_scale)
    return;

  dpi_scale_ = dpi_scale;
  SendScreenRectsAndResizeWidget();
}

void ContentViewCoreImpl::SetFocusInternal(bool focused) {
  if (!GetRenderWidgetHostViewAndroid())
    return;

  if (focused)
    GetRenderWidgetHostViewAndroid()->Focus();
  else
    GetRenderWidgetHostViewAndroid()->Blur();
}

void ContentViewCoreImpl::SendOrientationChangeEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint orientation) {
  if (device_orientation_ != orientation) {
    base::RecordAction(base::UserMetricsAction("ScreenOrientationChange"));
    device_orientation_ = orientation;
    SendOrientationChangeEventInternal();
  }
}

WebGestureEvent ContentViewCoreImpl::MakeGestureEvent(WebInputEvent::Type type,
                                                      int64_t time_ms,
                                                      float x,
                                                      float y) const {
  return WebGestureEventBuilder::Build(
      type, time_ms / 1000.0, x / dpi_scale(), y / dpi_scale());
}

void ContentViewCoreImpl::SendGestureEvent(
    const blink::WebGestureEvent& event) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SendGestureEvent(event);
}

void ContentViewCoreImpl::ScrollBegin(JNIEnv* env,
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

void ContentViewCoreImpl::ScrollEnd(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jlong time_ms) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::kGestureScrollEnd, time_ms, 0, 0);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::ScrollBy(JNIEnv* env,
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

void ContentViewCoreImpl::FlingStart(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     jlong time_ms,
                                     jfloat x,
                                     jfloat y,
                                     jfloat vx,
                                     jfloat vy,
                                     jboolean target_viewport,
                                     jboolean from_gamepad) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::kGestureFlingStart, time_ms, x, y);
  event.data.fling_start.velocity_x = vx / dpi_scale();
  event.data.fling_start.velocity_y = vy / dpi_scale();
  event.data.fling_start.target_viewport = target_viewport;

  if (from_gamepad)
    event.source_device = blink::kWebGestureDeviceSyntheticAutoscroll;

  SendGestureEvent(event);
}

void ContentViewCoreImpl::FlingCancel(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jlong time_ms,
                                      jboolean from_gamepad) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::kGestureFlingCancel, time_ms, 0, 0);
  event.data.fling_cancel.prevent_boosting = true;

  if (from_gamepad) {
    event.data.fling_cancel.target_viewport = true;
    event.source_device = blink::kWebGestureDeviceSyntheticAutoscroll;
  }

  SendGestureEvent(event);
}

void ContentViewCoreImpl::DoubleTap(JNIEnv* env,
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

void ContentViewCoreImpl::ResolveTapDisambiguation(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong time_ms,
    jfloat x,
    jfloat y,
    jboolean is_long_press) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (!rwhv)
    return;

  rwhv->ResolveTapDisambiguation(time_ms / 1000.0,
                                 gfx::Point(x / dpi_scale_, y / dpi_scale_),
                                 is_long_press);
}

void ContentViewCoreImpl::PinchBegin(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     jlong time_ms,
                                     jfloat x,
                                     jfloat y) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::kGesturePinchBegin, time_ms, x, y);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::PinchEnd(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jlong time_ms) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::kGesturePinchEnd, time_ms, 0, 0);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::PinchBy(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jlong time_ms,
                                  jfloat anchor_x,
                                  jfloat anchor_y,
                                  jfloat delta) {
  WebGestureEvent event = MakeGestureEvent(WebInputEvent::kGesturePinchUpdate,
                                           time_ms, anchor_x, anchor_y);
  event.data.pinch_update.scale = delta;

  SendGestureEvent(event);
}

void ContentViewCoreImpl::SetTextHandlesTemporarilyHidden(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean hidden) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SetTextHandlesTemporarilyHidden(hidden);
}

void ContentViewCoreImpl::ResetGestureDetection(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->ResetGestureDetection();
}

void ContentViewCoreImpl::SetDoubleTapSupportEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SetDoubleTapSupportEnabled(enabled);
}

void ContentViewCoreImpl::SetMultiTouchZoomSupportEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SetMultiTouchZoomSupportEnabled(enabled);
}

void ContentViewCoreImpl::OnTouchDown(
    const base::android::ScopedJavaLocalRef<jobject>& event) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_onTouchDown(env, obj, event);
}

void ContentViewCoreImpl::SetAllowJavascriptInterfacesInspection(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean allow) {
  java_bridge_dispatcher_host_->SetAllowObjectContentsInspection(allow);
}

void ContentViewCoreImpl::AddJavascriptInterface(
    JNIEnv* env,
    const JavaParamRef<jobject>& /* obj */,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jstring>& name,
    const JavaParamRef<jclass>& safe_annotation_clazz) {
  java_bridge_dispatcher_host_->AddNamedObject(
      ConvertJavaStringToUTF8(env, name), object, safe_annotation_clazz);
}

void ContentViewCoreImpl::RemoveJavascriptInterface(
    JNIEnv* env,
    const JavaParamRef<jobject>& /* obj */,
    const JavaParamRef<jstring>& name) {
  java_bridge_dispatcher_host_->RemoveNamedObject(
      ConvertJavaStringToUTF8(env, name));
}

void ContentViewCoreImpl::WasResized(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  SendScreenRectsAndResizeWidget();
}

void ContentViewCoreImpl::SetTextTrackSettings(
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
  params.text_track_background_color = ConvertJavaStringToUTF8(
      env, textTrackBackgroundColor);
  params.text_track_font_family = ConvertJavaStringToUTF8(
      env, textTrackFontFamily);
  params.text_track_font_style = ConvertJavaStringToUTF8(
      env, textTrackFontStyle);
  params.text_track_font_variant = ConvertJavaStringToUTF8(
      env, textTrackFontVariant);
  params.text_track_text_color = ConvertJavaStringToUTF8(
      env, textTrackTextColor);
  params.text_track_text_shadow = ConvertJavaStringToUTF8(
      env, textTrackTextShadow);
  params.text_track_text_size = ConvertJavaStringToUTF8(
      env, textTrackTextSize);
  web_contents_->GetMainFrame()->SetTextTrackSettings(params);
}

bool ContentViewCoreImpl::IsFullscreenRequiredForOrientationLock() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return true;
  return Java_ContentViewCore_isFullscreenRequiredForOrientationLock(env, obj);
}

void ContentViewCoreImpl::SendOrientationChangeEventInternal() {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->UpdateScreenInfo(GetViewAndroid());

  static_cast<WebContentsImpl*>(web_contents())->OnScreenOrientationChange();
}

jint ContentViewCoreImpl::GetCurrentRenderProcessId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetRenderProcessIdFromRenderViewHost(
      web_contents_->GetRenderViewHost());
}

void ContentViewCoreImpl::SetBackgroundOpaque(JNIEnv* env,
                                              const JavaParamRef<jobject>& jobj,
                                              jboolean opaque) {
  if (GetRenderWidgetHostViewAndroid()) {
    if (opaque)
      GetRenderWidgetHostViewAndroid()->SetBackgroundColorToDefault();
    else
      GetRenderWidgetHostViewAndroid()->SetBackgroundColor(SK_ColorTRANSPARENT);
  }
}

void ContentViewCoreImpl::HidePopupsAndPreserveSelection() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_ContentViewCore_hidePopupsAndPreserveSelection(env, obj);
}

void ContentViewCoreImpl::WebContentsDestroyed() {
  WebContentsViewAndroid* wcva = static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(web_contents())->GetView());
  DCHECK(wcva);
  wcva->SetContentViewCore(NULL);
}

// This is called for each ContentView.
jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& jweb_contents,
           const JavaParamRef<jobject>& jview_android_delegate,
           jlong jwindow_android,
           jfloat dip_scale,
           const JavaParamRef<jobject>& retained_objects_set) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromJavaWebContents(jweb_contents));
  CHECK(web_contents) <<
      "A ContentViewCoreImpl should be created with a valid WebContents.";
  ui::ViewAndroid* view_android = web_contents->GetView()->GetNativeView();
  view_android->SetDelegate(jview_android_delegate);
  view_android->SetLayout(ui::ViewAndroid::LayoutParams::MatchParent());

  ui::WindowAndroid* window_android =
        reinterpret_cast<ui::WindowAndroid*>(jwindow_android);
  DCHECK(window_android);
  window_android->AddChild(view_android);

  ContentViewCoreImpl* view = new ContentViewCoreImpl(
      env, obj, web_contents, dip_scale, retained_objects_set);
  return reinterpret_cast<intptr_t>(view);
}

static ScopedJavaLocalRef<jobject> FromWebContentsAndroid(
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

bool RegisterContentViewCore(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
