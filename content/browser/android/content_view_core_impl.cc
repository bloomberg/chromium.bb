// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_core_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "cc/layer.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/android/load_url_params.h"
#include "content/browser/android/touch_point.h"
#include "content/browser/renderer_host/java/java_bound_object.h"
#include "content/browser/renderer_host/java/java_bridge_dispatcher_host_manager.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/ssl/ssl_host_state.h"
#include "content/browser/web_contents/interstitial_page_impl.h"
#include "content/browser/web_contents/navigation_controller_impl.h"
#include "content/browser/web_contents/navigation_entry_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_transition_types.h"
#include "jni/ContentViewCore_jni.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/android/WebInputEventFactory.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/android/window_android.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/size_f.h"
#include "webkit/glue/webmenuitem.h"
#include "webkit/user_agent/user_agent_util.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::HasField;
using base::android::MethodID;
using base::android::JavaByteArrayToByteVector;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebInputEventFactory;

// Describes the type and enabled state of a select popup item.
// Keep in sync with the value defined in SelectPopupDialog.java
enum PopupItemType {
  POPUP_ITEM_TYPE_GROUP = 0,
  POPUP_ITEM_TYPE_DISABLED,
  POPUP_ITEM_TYPE_ENABLED
};

namespace content {

namespace {
jfieldID g_native_content_view;

const void* kContentViewUserDataKey = &kContentViewUserDataKey;

int GetRenderProcessIdFromRenderViewHost(RenderViewHost* host) {
  DCHECK(host);
  RenderProcessHost* render_process = host->GetProcess();
  DCHECK(render_process);
  if (render_process->HasConnection())
    return render_process->GetHandle();
  else
    return 0;
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

  virtual ~ContentViewUserData() {
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

struct ContentViewCoreImpl::JavaObject {
  ScopedJavaGlobalRef<jclass> rect_clazz;
  jmethodID rect_constructor;
  ScopedJavaLocalRef<jobject> CreateJavaRect(
      JNIEnv* env,
      const gfx::Rect& rect,
      float scale) {
    return ScopedJavaLocalRef<jobject>(
        env, env->NewObject(rect_clazz.obj(),
                            rect_constructor,
                            static_cast<int>(rect.x() * scale),
                            static_cast<int>(rect.y() * scale),
                            static_cast<int>(rect.right() * scale),
                            static_cast<int>(rect.bottom() * scale)));
  }
};

// static
ContentViewCoreImpl* ContentViewCoreImpl::FromWebContents(
    content::WebContents* web_contents) {
  ContentViewCoreImpl::ContentViewUserData* data =
      reinterpret_cast<ContentViewCoreImpl::ContentViewUserData*>(
          web_contents->GetUserData(kContentViewUserDataKey));
  return data ? data->get() : NULL;
}

// static
ContentViewCore* ContentViewCore::FromWebContents(
    content::WebContents* web_contents) {
  return ContentViewCoreImpl::FromWebContents(web_contents);
}

ContentViewCore* ContentViewCore::GetNativeContentViewCore(JNIEnv* env,
                                                           jobject obj) {
  return reinterpret_cast<ContentViewCore*>(
      env->GetIntField(obj, g_native_content_view));
}

ContentViewCoreImpl::ContentViewCoreImpl(JNIEnv* env, jobject obj,
                                         bool hardware_accelerated,
                                         bool input_events_delivered_at_vsync,
                                         WebContents* web_contents,
                                         ui::WindowAndroid* window_android)
    : java_ref_(env, obj),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)),
      root_layer_(cc::Layer::create()),
      tab_crashed_(false),
      input_events_delivered_at_vsync_(input_events_delivered_at_vsync),
      renderer_frame_pending_(false),
      window_android_(window_android) {
  CHECK(web_contents) <<
      "A ContentViewCoreImpl should be created with a valid WebContents.";

  // When a tab is restored (from a saved state), it does not have a renderer
  // process.  We treat it like the tab is crashed. If the content is loaded
  // when the tab is shown, tab_crashed_ will be reset.  Since
  // RenderWidgetHostView is associated with the lifetime of the renderer
  // process, we use it to test whether there is a renderer process.
  tab_crashed_ = !(web_contents->GetRenderWidgetHostView());

  // TODO(leandrogracia): make use of the hardware_accelerated argument.

  InitJNI(env, obj);

  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  dpi_scale_ = display.device_scale_factor();

  // Currently, the only use case we have for overriding a user agent involves
  // spoofing a desktop Linux user agent for "Request desktop site".
  // Automatically set it for all WebContents so that it is available when a
  // NavigationEntry requires the user agent to be overridden.
  const char kLinuxInfoStr[] = "X11; Linux x86_64";
  std::string product = content::GetContentClient()->GetProduct();
  std::string spoofed_ua =
      webkit_glue::BuildUserAgentFromOSAndProduct(kLinuxInfoStr, product);
  web_contents->SetUserAgentOverride(spoofed_ua);

  InitWebContents();
}

ContentViewCoreImpl::~ContentViewCoreImpl() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  java_ref_.reset();
  if (!j_obj.is_null()) {
    Java_ContentViewCore_onNativeContentViewCoreDestroyed(
        env, j_obj.obj(), reinterpret_cast<jint>(this));
  }
  // Make sure nobody calls back into this object while we are tearing things
  // down.
  notification_registrar_.RemoveAll();

  delete java_object_;
  java_object_ = NULL;
}

void ContentViewCoreImpl::OnJavaContentViewCoreDestroyed(JNIEnv* env,
                                                         jobject obj) {
  DCHECK(env->IsSameObject(java_ref_.get(env).obj(), obj));
  java_ref_.reset();
}

void ContentViewCoreImpl::InitWebContents() {
  DCHECK(web_contents_);
  notification_registrar_.Add(
      this, NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
      Source<NavigationController>(&web_contents_->GetController()));
  notification_registrar_.Add(
      this, NOTIFICATION_RENDERER_PROCESS_CREATED,
      content::NotificationService::AllBrowserContextsAndSources());
  notification_registrar_.Add(
      this, NOTIFICATION_WEB_CONTENTS_CONNECTED,
      Source<WebContents>(web_contents_));

  static_cast<WebContentsViewAndroid*>(web_contents_->GetView())->
      SetContentViewCore(this);
  DCHECK(!web_contents_->GetUserData(kContentViewUserDataKey));
  web_contents_->SetUserData(kContentViewUserDataKey,
                             new ContentViewUserData(this));
}

void ContentViewCoreImpl::Observe(int type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RENDER_VIEW_HOST_CHANGED: {
      std::pair<RenderViewHost*, RenderViewHost*>* switched_details =
          Details<std::pair<RenderViewHost*, RenderViewHost*> >(details).ptr();
      int old_pid = 0;
      if (switched_details->first) {
        old_pid = GetRenderProcessIdFromRenderViewHost(
            switched_details->first);
      }
      int new_pid = GetRenderProcessIdFromRenderViewHost(
          web_contents_->GetRenderViewHost());
      if (new_pid != old_pid) {
        // Notify the Java side of the change of the current renderer process.
        JNIEnv* env = AttachCurrentThread();
        ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
        if (!obj.is_null()) {
          Java_ContentViewCore_onRenderProcessSwap(
              env, obj.obj(), old_pid, new_pid);
        }
      }
      break;
    }
    case NOTIFICATION_RENDERER_PROCESS_CREATED: {
      // Notify the Java side of the current renderer process.
      RenderProcessHost* source_process_host =
          Source<RenderProcessHost>(source).ptr();
      RenderProcessHost* current_process_host =
          web_contents_->GetRenderViewHost()->GetProcess();

      if (source_process_host == current_process_host) {
        int pid = GetRenderProcessIdFromRenderViewHost(
            web_contents_->GetRenderViewHost());
        JNIEnv* env = AttachCurrentThread();
        ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
        if (!obj.is_null()) {
          Java_ContentViewCore_onRenderProcessSwap(env, obj.obj(), 0, pid);
        }
      }
      break;
    }
    case NOTIFICATION_WEB_CONTENTS_CONNECTED: {
      JNIEnv* env = AttachCurrentThread();
      ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
      if (!obj.is_null()) {
        Java_ContentViewCore_onWebContentsConnected(env, obj.obj());
      }
      break;
    }
  }
}

void ContentViewCoreImpl::InitJNI(JNIEnv* env, jobject obj) {
  java_object_ = new JavaObject;
  java_object_->rect_clazz.Reset(GetClass(env, "android/graphics/Rect"));
  java_object_->rect_constructor = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, java_object_->rect_clazz.obj(), "<init>", "(IIII)V");
}

RenderWidgetHostViewAndroid*
    ContentViewCoreImpl::GetRenderWidgetHostViewAndroid() {
  RenderWidgetHostView* rwhv = NULL;
  if (web_contents_) {
    rwhv = web_contents_->GetRenderWidgetHostView();
    if (web_contents_->ShowingInterstitialPage()) {
      rwhv = static_cast<InterstitialPageImpl*>(
          web_contents_->GetInterstitialPage())->
              GetRenderViewHost()->GetView();
    }
  }
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

ScopedJavaLocalRef<jobject> ContentViewCoreImpl::GetJavaObject() {
  JNIEnv* env = AttachCurrentThread();
  return java_ref_.get(env);
}

ScopedJavaLocalRef<jobject> ContentViewCoreImpl::GetContainerViewDelegate() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return ScopedJavaLocalRef<jobject>();
  return Java_ContentViewCore_getContainerViewDelegate(env, obj.obj());
}

void ContentViewCoreImpl::OnWebPreferencesUpdated() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_onWebPreferencesUpdated(env, obj.obj());
}

jint ContentViewCoreImpl::GetBackgroundColor(JNIEnv* env, jobject obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return SK_ColorWHITE;
  return rwhva->GetCachedBackgroundColor();
}

void ContentViewCoreImpl::SetBackgroundColor(JNIEnv* env,
                                             jobject obj,
                                             jint color) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return;
  rwhva->SetCachedBackgroundColor(color);
}

void ContentViewCoreImpl::OnHide(JNIEnv* env, jobject obj) {
  Hide();
}

void ContentViewCoreImpl::OnShow(JNIEnv* env, jobject obj) {
  Show();
}

void ContentViewCoreImpl::Show() {
  GetWebContents()->WasShown();
}

void ContentViewCoreImpl::Hide() {
  GetWebContents()->WasHidden();
}

void ContentViewCoreImpl::OnTabCrashed() {
  // if tab_crashed_ is already true, just return. e.g. if two tabs share the
  // render process, this will be called for each tab when the render process
  // crashed. If user reload one tab, a new render process is created. It can be
  // shared by the other tab. But if user closes the tab before reload the other
  // tab, the new render process will be shut down. This will trigger the other
  // tab's OnTabCrashed() called again as two tabs share the same
  // BrowserRenderProcessHost.
  if (tab_crashed_)
    return;
  tab_crashed_ = true;
  // This can happen as WebContents is destroyed after java_object_ is set to 0
  if (!java_object_)
    return;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_onTabCrash(env, obj.obj());
}

void ContentViewCoreImpl::UpdateContentSize(int width, int height) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_updateContentSize(env, obj.obj(), width, height);
}

void ContentViewCoreImpl::UpdateScrollOffsetAndPageScaleFactor(int x, int y,
                                                               float scale) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_updateScrollOffsetAndPageScaleFactor(
      env, obj.obj(), static_cast<int>(x * GetDpiScale()),
      static_cast<int>(y * GetDpiScale()), scale);
}

void ContentViewCoreImpl::UpdatePageScaleLimits(float minimum_scale,
                                                float maximum_scale) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_updatePageScaleLimits(env, obj.obj(), minimum_scale,
                                             maximum_scale);
}

void ContentViewCoreImpl::UpdateOffsetsForFullscreen(float controls_offset_y,
                                                     float content_offset_y) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_updateOffsetsForFullscreen(env, obj.obj(),
                                                  controls_offset_y,
                                                  content_offset_y);
}

void ContentViewCoreImpl::SetTitle(const string16& title) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jtitle =
      ConvertUTF8ToJavaString(env, UTF16ToUTF8(title));
  Java_ContentViewCore_setTitle(env, obj.obj(), jtitle.obj());
}

void ContentViewCoreImpl::OnBackgroundColorChanged(SkColor color) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_onBackgroundColorChanged(env, obj.obj(), color);
}

void ContentViewCoreImpl::ShowSelectPopupMenu(
    const std::vector<WebMenuItem>& items, int selected_item, bool multiple) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;

  // For multi-select list popups we find the list of previous selections by
  // iterating through the items. But for single selection popups we take the
  // given |selected_item| as is.
  ScopedJavaLocalRef<jintArray> selected_array;
  if (multiple) {
    scoped_array<jint> native_selected_array(new jint[items.size()]);
    size_t selected_count = 0;
    for (size_t i = 0; i < items.size(); ++i) {
      if (items[i].checked)
        native_selected_array[selected_count++] = i;
    }

    selected_array.Reset(env, env->NewIntArray(selected_count));
    env->SetIntArrayRegion(selected_array.obj(), 0, selected_count,
                           native_selected_array.get());
  } else {
    selected_array.Reset(env, env->NewIntArray(1));
    jint value = selected_item;
    env->SetIntArrayRegion(selected_array.obj(), 0, 1, &value);
  }

  ScopedJavaLocalRef<jintArray> enabled_array(env,
                                              env->NewIntArray(items.size()));
  std::vector<string16> labels;
  labels.reserve(items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    labels.push_back(items[i].label);
    jint enabled =
        (items[i].type == WebMenuItem::GROUP ? POPUP_ITEM_TYPE_GROUP :
            (items[i].enabled ? POPUP_ITEM_TYPE_ENABLED :
                POPUP_ITEM_TYPE_DISABLED));
    env->SetIntArrayRegion(enabled_array.obj(), i, 1, &enabled);
  }
  ScopedJavaLocalRef<jobjectArray> items_array(
      base::android::ToJavaArrayOfStrings(env, labels));
  Java_ContentViewCore_showSelectPopup(env, j_obj.obj(),
                                       items_array.obj(), enabled_array.obj(),
                                       multiple, selected_array.obj());
}

void ContentViewCoreImpl::ConfirmTouchEvent(InputEventAckState ack_result) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_ContentViewCore_confirmTouchEvent(env, j_obj.obj(),
                                         static_cast<jint>(ack_result));
}

void ContentViewCoreImpl::HasTouchEventHandlers(bool need_touch_events) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_ContentViewCore_hasTouchEventHandlers(env,
                                             j_obj.obj(),
                                             need_touch_events);
}

bool ContentViewCoreImpl::HasFocus() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_ContentViewCore_hasFocus(env, obj.obj());
}

void ContentViewCoreImpl::OnSelectionChanged(const std::string& text) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jtext = ConvertUTF8ToJavaString(env, text);
  Java_ContentViewCore_onSelectionChanged(env, obj.obj(), jtext.obj());
}

void ContentViewCoreImpl::OnSelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jobject> anchor_rect(
      java_object_->CreateJavaRect(env, params.anchor_rect, 1.f));
  ScopedJavaLocalRef<jobject> focus_rect(
      java_object_->CreateJavaRect(env, params.focus_rect, 1.f));
  Java_ContentViewCore_onSelectionBoundsChanged(env, obj.obj(),
                                                anchor_rect.obj(),
                                                params.anchor_dir,
                                                focus_rect.obj(),
                                                params.focus_dir,
                                                params.is_anchor_first);
}

void ContentViewCoreImpl::ShowPastePopup(int x, int y) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_showPastePopup(env, obj.obj(),
                                      static_cast<jint>(x),
                                      static_cast<jint>(y));
}

unsigned int ContentViewCoreImpl::GetScaledContentTexture(
    float scale,
    gfx::Size* out_size) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (!view)
    return 0;

  return view->GetScaledContentTexture(scale, out_size);
}

void ContentViewCoreImpl::StartContentIntent(const GURL& content_url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jcontent_url =
      ConvertUTF8ToJavaString(env, content_url.spec());
  Java_ContentViewCore_startContentIntent(env,
                                          j_obj.obj(),
                                          jcontent_url.obj());
}

void ContentViewCoreImpl::ShowDisambiguationPopup(
    const gfx::Rect& target_rect,
    const SkBitmap& zoomed_bitmap) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jobject> rect_object(env,
      env->NewObject(java_object_->rect_clazz.obj(),
                     java_object_->rect_constructor,
                     target_rect.x(),
                     target_rect.y(),
                     target_rect.right(),
                     target_rect.bottom()));
  DCHECK(!rect_object.is_null());

  ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(&zoomed_bitmap);
  DCHECK(!java_bitmap.is_null());

  Java_ContentViewCore_showDisambiguationPopup(env,
                                               obj.obj(),
                                               rect_object.obj(),
                                               java_bitmap.obj());
}

gfx::Size ContentViewCoreImpl::GetPhysicalSize() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return gfx::Size();
  return gfx::Size(Java_ContentViewCore_getWidth(env, j_obj.obj()),
                   Java_ContentViewCore_getHeight(env, j_obj.obj()));
}

gfx::Size ContentViewCoreImpl::GetDIPSize() const {
  return gfx::ToCeiledSize(
      gfx::ScaleSize(GetPhysicalSize(), 1 / GetDpiScale()));
}

void ContentViewCoreImpl::AttachLayer(scoped_refptr<cc::Layer> layer) {
  root_layer_->addChild(layer);
}

void ContentViewCoreImpl::RemoveLayer(scoped_refptr<cc::Layer> layer) {
  layer->removeFromParent();
}

void ContentViewCoreImpl::DidProduceRendererFrame() {
  renderer_frame_pending_ = true;
}

void ContentViewCoreImpl::LoadUrl(
    NavigationController::LoadURLParams& params) {
  GetWebContents()->GetController().LoadURLWithParams(params);
  tab_crashed_ = false;
}

ui::WindowAndroid* ContentViewCoreImpl::GetWindowAndroid() const {
  return window_android_;
}

scoped_refptr<cc::Layer> ContentViewCoreImpl::GetLayer() const {
  return root_layer_.get();
}

// ----------------------------------------------------------------------------
// Methods called from Java via JNI
// ----------------------------------------------------------------------------

void ContentViewCoreImpl::SelectPopupMenuItems(JNIEnv* env, jobject obj,
                                               jintArray indices) {
  RenderViewHostImpl* rvhi = static_cast<RenderViewHostImpl*>(
      web_contents_->GetRenderViewHost());
  DCHECK(rvhi);
  if (indices == NULL) {
    rvhi->DidCancelPopupMenu();
    return;
  }

  int selected_count = env->GetArrayLength(indices);
  std::vector<int> selected_indices;
  jint* indices_ptr = env->GetIntArrayElements(indices, NULL);
  for (int i = 0; i < selected_count; ++i)
    selected_indices.push_back(indices_ptr[i]);
  env->ReleaseIntArrayElements(indices, indices_ptr, JNI_ABORT);
  rvhi->DidSelectPopupMenuItems(selected_indices);
}

void ContentViewCoreImpl::LoadUrl(
    JNIEnv* env, jobject obj,
    jstring url,
    jint load_url_type,
    jint transition_type,
    jint ua_override_option,
    jstring extra_headers,
    jbyteArray post_data,
    jstring base_url_for_data_url,
    jstring virtual_url_for_data_url,
    jboolean can_load_local_resources) {
  DCHECK(url);
  NavigationController::LoadURLParams params(
      GURL(ConvertJavaStringToUTF8(env, url)));

  params.load_type = static_cast<NavigationController::LoadURLType>(
      load_url_type);
  params.transition_type = PageTransitionFromInt(transition_type);
  params.override_user_agent =
      static_cast<NavigationController::UserAgentOverrideOption>(
          ua_override_option);

  if (extra_headers)
    params.extra_headers = ConvertJavaStringToUTF8(env, extra_headers);

  if (post_data) {
    std::vector<uint8> http_body_vector;
    JavaByteArrayToByteVector(env, post_data, &http_body_vector);
    params.browser_initiated_post_data =
        base::RefCountedBytes::TakeVector(&http_body_vector);
  }

  if (base_url_for_data_url) {
    params.base_url_for_data_url =
        GURL(ConvertJavaStringToUTF8(env, base_url_for_data_url));
  }

  if (virtual_url_for_data_url) {
    params.virtual_url_for_data_url =
        GURL(ConvertJavaStringToUTF8(env, virtual_url_for_data_url));
  }

  params.can_load_local_resources = can_load_local_resources;

  LoadUrl(params);
}

jint ContentViewCoreImpl::GetCurrentRenderProcessId(JNIEnv* env, jobject obj) {
  return GetRenderProcessIdFromRenderViewHost(
      web_contents_->GetRenderViewHost());
}

void ContentViewCoreImpl::SetAllUserAgentOverridesInHistory(
    JNIEnv* env,
    jobject,
    jstring user_agent_override) {
  std::string override =
      base::android::ConvertJavaStringToUTF8(env, user_agent_override);
  web_contents_->SetUserAgentOverride(override);
  bool override_used = !override.empty();
  const NavigationController& controller = web_contents_->GetController();
  for (int i = 0; i < controller.GetEntryCount(); ++i)
    controller.GetEntryAtIndex(i)->SetIsOverridingUserAgent(override_used);
}

ScopedJavaLocalRef<jstring> ContentViewCoreImpl::GetURL(
    JNIEnv* env, jobject) const {
  return ConvertUTF8ToJavaString(env, GetWebContents()->GetURL().spec());
}

ScopedJavaLocalRef<jstring> ContentViewCoreImpl::GetTitle(
    JNIEnv* env, jobject obj) const {
  return ConvertUTF16ToJavaString(env, GetWebContents()->GetTitle());
}

jboolean ContentViewCoreImpl::IsIncognito(JNIEnv* env, jobject obj) {
  return GetWebContents()->GetBrowserContext()->IsOffTheRecord();
}

WebContents* ContentViewCoreImpl::GetWebContents() const {
  return web_contents_;
}

void ContentViewCoreImpl::SetFocus(JNIEnv* env, jobject obj, jboolean focused) {
  if (!GetRenderWidgetHostViewAndroid())
    return;

  if (focused)
    GetRenderWidgetHostViewAndroid()->Focus();
  else
    GetRenderWidgetHostViewAndroid()->Blur();
}

void ContentViewCoreImpl::SendOrientationChangeEvent(JNIEnv* env,
                                                     jobject obj,
                                                     jint orientation) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->UpdateScreenInfo(rwhv->GetNativeView());

  RenderViewHostImpl* rvhi = static_cast<RenderViewHostImpl*>(
      web_contents_->GetRenderViewHost());
  rvhi->SendOrientationChangeEvent(orientation);
}

jboolean ContentViewCoreImpl::SendTouchEvent(JNIEnv* env,
                                             jobject obj,
                                             jlong time_ms,
                                             jint type,
                                             jobjectArray pts) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv) {
    using WebKit::WebTouchEvent;
    WebKit::WebTouchEvent event;
    TouchPoint::BuildWebTouchEvent(env, type, time_ms, GetDpiScale(), pts,
        event);
    UpdateVSyncFlagOnInputEvent(&event);
    rwhv->SendTouchEvent(event);
    return true;
  }
  return false;
}

int ContentViewCoreImpl::GetTouchPadding()
{
  // TODO(trchen): derive a proper padding value from device dpi
  return 48;
}

float ContentViewCoreImpl::GetDpiScale() const {
  return dpi_scale_;
}

jboolean ContentViewCoreImpl::SendMouseMoveEvent(JNIEnv* env,
                                                 jobject obj,
                                                 jlong time_ms,
                                                 jint x,
                                                 jint y) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (!rwhv)
    return false;

  WebKit::WebMouseEvent event = WebInputEventFactory::mouseEvent(
      WebInputEventFactory::MouseEventTypeMove,
      WebKit::WebMouseEvent::ButtonNone,
      time_ms / 1000.0, x / GetDpiScale(), y / GetDpiScale(), 0, 1);

  rwhv->SendMouseEvent(event);
  return true;
}

jboolean ContentViewCoreImpl::SendMouseWheelEvent(JNIEnv* env,
                                                  jobject obj,
                                                  jlong time_ms,
                                                  jint x,
                                                  jint y,
                                                  jfloat vertical_axis) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (!rwhv)
    return false;

  WebKit::WebInputEventFactory::MouseWheelDirectionType type;
  if (vertical_axis > 0) {
    type = WebInputEventFactory::MouseWheelDirectionTypeUp;
  } else if (vertical_axis < 0) {
    type = WebInputEventFactory::MouseWheelDirectionTypeDown;
  } else {
    return false;
  }
  WebKit::WebMouseWheelEvent event = WebInputEventFactory::mouseWheelEvent(
      type, time_ms / 1000.0, x / GetDpiScale(), y / GetDpiScale());

  rwhv->SendMouseWheelEvent(event);
  return true;
}

WebGestureEvent ContentViewCoreImpl::MakeGestureEvent(WebInputEvent::Type type,
                                                      long time_ms,
                                                      int x, int y) const {
  WebGestureEvent event;
  event.type = type;
  event.x = x / GetDpiScale();
  event.y = y / GetDpiScale();
  event.timeStampSeconds = time_ms / 1000.0;
  event.sourceDevice = WebGestureEvent::Touchscreen;
  UpdateVSyncFlagOnInputEvent(&event);
  return event;
}

void ContentViewCoreImpl::UpdateVSyncFlagOnInputEvent(
    WebKit::WebInputEvent* event) const {
  if (!input_events_delivered_at_vsync_)
    return;
  if (event->type == WebInputEvent::GestureScrollUpdate ||
      event->type == WebInputEvent::GesturePinchUpdate ||
      event->type == WebInputEvent::TouchMove)
    event->modifiers |= WebInputEvent::IsLastInputEventForCurrentVSync;
}

void ContentViewCoreImpl::SendGestureEvent(
    const WebKit::WebGestureEvent& event) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SendGestureEvent(event);
}

void ContentViewCoreImpl::ScrollBegin(JNIEnv* env, jobject obj, jlong time_ms,
                                      jint x, jint y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureScrollBegin, time_ms, x, y);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::ScrollEnd(JNIEnv* env, jobject obj, jlong time_ms) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureScrollEnd, time_ms, 0, 0);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::ScrollBy(JNIEnv* env, jobject obj, jlong time_ms,
                                   jint x, jint y, jint dx, jint dy) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureScrollUpdate, time_ms, x, y);

  event.data.scrollUpdate.deltaX = -dx / GetDpiScale();
  event.data.scrollUpdate.deltaY = -dy / GetDpiScale();

  SendGestureEvent(event);
}

void ContentViewCoreImpl::FlingStart(JNIEnv* env, jobject obj, jlong time_ms,
                                     jint x, jint y, jint vx, jint vy) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureFlingStart, time_ms, x, y);
  event.data.flingStart.velocityX = vx;
  event.data.flingStart.velocityY = vy;

  SendGestureEvent(event);
}

void ContentViewCoreImpl::FlingCancel(JNIEnv* env, jobject obj, jlong time_ms) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureFlingCancel, time_ms, 0, 0);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::SingleTap(JNIEnv* env, jobject obj, jlong time_ms,
                                    jint x, jint y,
                                    jboolean disambiguation_popup_tap) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureTap, time_ms, x, y);

  event.data.tap.tapCount = 1;
  if (!disambiguation_popup_tap) {
    int touchPadding = GetTouchPadding();
    event.data.tap.width = touchPadding / GetDpiScale();
    event.data.tap.height = touchPadding / GetDpiScale();
  }

  SendGestureEvent(event);
}

void ContentViewCoreImpl::ShowPressState(JNIEnv* env, jobject obj,
                                         jlong time_ms,
                                         jint x, jint y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureTapDown, time_ms, x, y);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::ShowPressCancel(JNIEnv* env,
                                          jobject obj,
                                          jlong time_ms,
                                          jint x,
                                          jint y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureTapCancel, time_ms, x, y);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::DoubleTap(JNIEnv* env, jobject obj, jlong time_ms,
                                    jint x, jint y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureDoubleTap, time_ms, x, y);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::LongPress(JNIEnv* env, jobject obj, jlong time_ms,
                                    jint x, jint y,
                                    jboolean disambiguation_popup_tap) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureLongPress, time_ms, x, y);

  if (!disambiguation_popup_tap) {
    int touchPadding = GetTouchPadding();
    event.data.longPress.width = event.data.longPress.height =
        touchPadding / GetDpiScale();
  }

  SendGestureEvent(event);
}

void ContentViewCoreImpl::LongTap(JNIEnv* env, jobject obj, jlong time_ms,
                                  jint x, jint y,
                                  jboolean disambiguation_popup_tap) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureLongTap, time_ms, x, y);

  if (!disambiguation_popup_tap) {
    int touchPadding = GetTouchPadding();
    event.data.longPress.width = event.data.longPress.height =
        touchPadding / GetDpiScale();
  }

  SendGestureEvent(event);
}

void ContentViewCoreImpl::PinchBegin(JNIEnv* env, jobject obj, jlong time_ms,
                                     jint x, jint y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GesturePinchBegin, time_ms, x, y);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::PinchEnd(JNIEnv* env, jobject obj, jlong time_ms) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GesturePinchEnd, time_ms, 0, 0);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::PinchBy(JNIEnv* env, jobject obj, jlong time_ms,
                                  jint anchor_x, jint anchor_y, jfloat delta) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GesturePinchUpdate, time_ms, anchor_x, anchor_y);
  event.data.pinchUpdate.scale = delta;

  SendGestureEvent(event);
}

void ContentViewCoreImpl::SelectBetweenCoordinates(JNIEnv* env, jobject obj,
                                                   jint x1, jint y1,
                                                   jint x2, jint y2) {
  if (GetRenderWidgetHostViewAndroid()) {
    GetRenderWidgetHostViewAndroid()->SelectRange(
        gfx::Point(x1 / GetDpiScale(), y1 / GetDpiScale()),
        gfx::Point(x2 / GetDpiScale(), y2 / GetDpiScale()));
  }
}

void ContentViewCoreImpl::MoveCaret(JNIEnv* env, jobject obj,
                                    jint x, jint y) {
  if (GetRenderWidgetHostViewAndroid()) {
    GetRenderWidgetHostViewAndroid()->MoveCaret(
        gfx::Point(x / GetDpiScale(), y / GetDpiScale()));
  }
}

jboolean ContentViewCoreImpl::CanGoBack(JNIEnv* env, jobject obj) {
  return web_contents_->GetController().CanGoBack();
}

jboolean ContentViewCoreImpl::CanGoForward(JNIEnv* env, jobject obj) {
  return web_contents_->GetController().CanGoForward();
}

jboolean ContentViewCoreImpl::CanGoToOffset(JNIEnv* env, jobject obj,
                                            jint offset) {
  return web_contents_->GetController().CanGoToOffset(offset);
}

void ContentViewCoreImpl::GoBack(JNIEnv* env, jobject obj) {
  web_contents_->GetController().GoBack();
  tab_crashed_ = false;
}

void ContentViewCoreImpl::GoForward(JNIEnv* env, jobject obj) {
  web_contents_->GetController().GoForward();
  tab_crashed_ = false;
}

void ContentViewCoreImpl::GoToOffset(JNIEnv* env, jobject obj, jint offset) {
  web_contents_->GetController().GoToOffset(offset);
}

void ContentViewCoreImpl::GoToNavigationIndex(JNIEnv* env,
                                              jobject obj,
                                              jint index) {
  web_contents_->GetController().GoToIndex(index);
}

void ContentViewCoreImpl::StopLoading(JNIEnv* env, jobject obj) {
  web_contents_->Stop();
}

void ContentViewCoreImpl::Reload(JNIEnv* env, jobject obj) {
  // Set check_for_repost parameter to false as we have no repost confirmation
  // dialog ("confirm form resubmission" screen will still appear, however).
  if (web_contents_->GetController().NeedsReload())
    web_contents_->GetController().LoadIfNecessary();
  else
    web_contents_->GetController().Reload(true);
  tab_crashed_ = false;
}

void ContentViewCoreImpl::CancelPendingReload(JNIEnv* env, jobject obj) {
  web_contents_->GetController().CancelPendingReload();
}

void ContentViewCoreImpl::ContinuePendingReload(JNIEnv* env, jobject obj) {
  web_contents_->GetController().ContinuePendingReload();
}

void ContentViewCoreImpl::ClearHistory(JNIEnv* env, jobject obj) {
  web_contents_->GetController().PruneAllButActive();
}

void ContentViewCoreImpl::AddJavascriptInterface(
    JNIEnv* env,
    jobject /* obj */,
    jobject object,
    jstring name,
    jclass safe_annotation_clazz,
    jobject retained_object_set) {
  ScopedJavaLocalRef<jobject> scoped_object(env, object);
  ScopedJavaLocalRef<jclass> scoped_clazz(env, safe_annotation_clazz);
  JavaObjectWeakGlobalRef weak_retained_object_set(env, retained_object_set);

  // JavaBoundObject creates the NPObject with a ref count of 1, and
  // JavaBridgeDispatcherHostManager takes its own ref.
  JavaBridgeDispatcherHostManager* java_bridge =
      web_contents_->java_bridge_dispatcher_host_manager();
  java_bridge->SetRetainedObjectSet(weak_retained_object_set);
  NPObject* bound_object = JavaBoundObject::Create(scoped_object, scoped_clazz,
                                                   java_bridge->AsWeakPtr());
  java_bridge->AddNamedObject(ConvertJavaStringToUTF16(env, name),
                              bound_object);
  WebKit::WebBindings::releaseObject(bound_object);
}

void ContentViewCoreImpl::RemoveJavascriptInterface(JNIEnv* env,
                                                    jobject /* obj */,
                                                    jstring name) {
  web_contents_->java_bridge_dispatcher_host_manager()->RemoveNamedObject(
      ConvertJavaStringToUTF16(env, name));
}

void ContentViewCoreImpl::UpdateVSyncParameters(JNIEnv* env, jobject /* obj */,
                                                jlong timebase_micros,
                                                jlong interval_micros) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (!view)
    return;

  RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
      view->GetRenderWidgetHost());

  host->UpdateVSyncParameters(
      base::TimeTicks::FromInternalValue(timebase_micros),
      base::TimeDelta::FromMicroseconds(interval_micros));
}

jboolean ContentViewCoreImpl::PopulateBitmapFromCompositor(JNIEnv* env,
                                                           jobject obj,
                                                           jobject jbitmap) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (!view)
    return false;

  return view->PopulateBitmapWithContents(jbitmap);
}

void ContentViewCoreImpl::SetSize(JNIEnv* env,
                                  jobject obj,
                                  jint width,
                                  jint height) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (!view)
    return;

  view->SetSize(gfx::Size(width, height));
}

void ContentViewCoreImpl::ShowInterstitialPage(
    JNIEnv* env, jobject obj, jstring jurl, jint delegate_ptr) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  InterstitialPageDelegateAndroid* delegate =
      reinterpret_cast<InterstitialPageDelegateAndroid*>(delegate_ptr);
  InterstitialPage* interstitial = InterstitialPage::Create(
      web_contents_, false, url, delegate);
  delegate->set_interstitial_page(interstitial);
  interstitial->Show();
}

jboolean ContentViewCoreImpl::IsShowingInterstitialPage(JNIEnv* env,
                                                        jobject obj) {
  return web_contents_->ShowingInterstitialPage();
}

jboolean ContentViewCoreImpl::ConsumePendingRendererFrame(JNIEnv* env,
                                                          jobject obj) {
  bool had_pending_frame = renderer_frame_pending_;
  renderer_frame_pending_ = false;
  return had_pending_frame;
}

jboolean ContentViewCoreImpl::IsRenderWidgetHostViewReady(JNIEnv* env,
                                                          jobject obj) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  return view && view->HasValidFrame();
}

void ContentViewCoreImpl::ExitFullscreen(JNIEnv* env, jobject obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  host->ExitFullscreen();
}

void ContentViewCoreImpl::ScrollFocusedEditableNodeIntoView(JNIEnv* env,
                                                            jobject obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  host->Send(new ViewMsg_ScrollFocusedEditableNodeIntoRect(host->GetRoutingID(),
                                                           gfx::Rect()));
}

namespace {

static void AddNavigationEntryToHistory(JNIEnv* env, jobject obj,
                                        jobject history,
                                        NavigationEntry* entry,
                                        int index) {
  // Get the details of the current entry
  ScopedJavaLocalRef<jstring> j_url(
      ConvertUTF8ToJavaString(env, entry->GetURL().spec()));
  ScopedJavaLocalRef<jstring> j_virtual_url(
      ConvertUTF8ToJavaString(env, entry->GetVirtualURL().spec()));
  ScopedJavaLocalRef<jstring> j_original_url(
      ConvertUTF8ToJavaString(env, entry->GetOriginalRequestURL().spec()));
  ScopedJavaLocalRef<jstring> j_title(
      ConvertUTF16ToJavaString(env, entry->GetTitle()));
  ScopedJavaLocalRef<jobject> j_bitmap;
  const FaviconStatus& status = entry->GetFavicon();
  if (status.valid && status.image.ToSkBitmap()->getSize() > 0)
    j_bitmap = gfx::ConvertToJavaBitmap(status.image.ToSkBitmap());

  // Add the item to the list
  Java_ContentViewCore_addToNavigationHistory(
      env, obj, history, index, j_url.obj(), j_virtual_url.obj(),
      j_original_url.obj(), j_title.obj(), j_bitmap.obj());
}

}  // namespace

int ContentViewCoreImpl::GetNavigationHistory(JNIEnv* env,
                                              jobject obj,
                                              jobject context) {
  // Iterate through navigation entries to populate the list
  const NavigationController& controller = web_contents_->GetController();
  int count = controller.GetEntryCount();
  for (int i = 0; i < count; ++i) {
    AddNavigationEntryToHistory(
        env, obj, context, controller.GetEntryAtIndex(i), i);
  }

  return controller.GetCurrentEntryIndex();
}

void ContentViewCoreImpl::GetDirectedNavigationHistory(JNIEnv* env,
                                                       jobject obj,
                                                       jobject context,
                                                       jboolean is_forward,
                                                       jint max_entries) {
  // Iterate through navigation entries to populate the list
  const NavigationController& controller = web_contents_->GetController();
  int count = controller.GetEntryCount();
  int num_added = 0;
  int increment_value = is_forward ? 1 : -1;
  for (int i = controller.GetCurrentEntryIndex() + increment_value;
       i >= 0 && i < count;
       i += increment_value) {
    if (num_added >= max_entries)
      break;

    AddNavigationEntryToHistory(
        env, obj, context, controller.GetEntryAtIndex(i), i);
    num_added++;
  }
}

int ContentViewCoreImpl::GetNativeImeAdapter(JNIEnv* env, jobject obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return 0;
  return rwhva->GetNativeImeAdapter();
}

jboolean ContentViewCoreImpl::NeedsReload(JNIEnv* env, jobject obj) {
  return web_contents_->GetController().NeedsReload();
}

void ContentViewCoreImpl::UndoScrollFocusedEditableNodeIntoView(
    JNIEnv* env,
    jobject obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  host->Send(
      new ViewMsg_UndoScrollFocusedEditableNodeIntoView(host->GetRoutingID()));
}

namespace {
void JavaScriptResultCallback(const ScopedJavaGlobalRef<jobject>& callback,
                              const base::Value* result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json;
  base::JSONWriter::Write(result, &json);
  ScopedJavaLocalRef<jstring> j_json = ConvertUTF8ToJavaString(env, json);
  Java_ContentViewCore_onEvaluateJavaScriptResult(env,
                                                  j_json.obj(),
                                                  callback.obj());
}
}  // namespace

void ContentViewCoreImpl::EvaluateJavaScript(JNIEnv* env,
                                             jobject obj,
                                             jstring script,
                                             jobject callback) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  DCHECK(host);

  if (!callback) {
    // No callback requested.
    host->ExecuteJavascriptInWebFrame(string16(),  // frame_xpath
                                      ConvertJavaStringToUTF16(env, script));
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  content::RenderViewHost::JavascriptResultCallback c_callback =
      base::Bind(&JavaScriptResultCallback, j_callback);

  host->ExecuteJavascriptInWebFrameCallbackResult(
      string16(),  // frame_xpath
      ConvertJavaStringToUTF16(env, script),
      c_callback);
}

bool ContentViewCoreImpl::GetUseDesktopUserAgent(
    JNIEnv* env, jobject obj) {
  NavigationEntry* entry = web_contents_->GetController().GetActiveEntry();
  return entry && entry->GetIsOverridingUserAgent();
}

void ContentViewCoreImpl::ImeUpdateAdapter(int native_ime_adapter,
                                           int text_input_type,
                                           const std::string& text,
                                           int selection_start,
                                           int selection_end,
                                           int composition_start,
                                           int composition_end,
                                           bool show_ime_if_needed) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jstring_text = ConvertUTF8ToJavaString(env, text);
  Java_ContentViewCore_imeUpdateAdapter(env, obj.obj(),
                                        native_ime_adapter, text_input_type,
                                        jstring_text.obj(),
                                        selection_start, selection_end,
                                        composition_start, composition_end,
                                        show_ime_if_needed);
}

void ContentViewCoreImpl::ClearSslPreferences(JNIEnv* env, jobject obj) {
  SSLHostState* state = SSLHostState::GetFor(
      web_contents_->GetController().GetBrowserContext());
  state->Clear();
}

void ContentViewCoreImpl::SetUseDesktopUserAgent(
    JNIEnv* env,
    jobject obj,
    jboolean enabled,
    jboolean reload_on_state_change) {
  if (GetUseDesktopUserAgent(env, obj) == enabled)
    return;

  // Make sure the navigation entry actually exists.
  NavigationEntry* entry = web_contents_->GetController().GetActiveEntry();
  if (!entry)
    return;

  // Set the flag in the NavigationEntry.
  entry->SetIsOverridingUserAgent(enabled);

  // Send the override to the renderer.
  if (reload_on_state_change) {
    // Reloading the page will send the override down as part of the
    // navigation IPC message.
    NavigationControllerImpl& controller =
        static_cast<NavigationControllerImpl&>(web_contents_->GetController());
    controller.ReloadOriginalRequestURL(false);
  }
}

// This is called for each ContentView.
jint Init(JNIEnv* env, jobject obj,
          jboolean input_events_delivered_at_vsync,
          jboolean hardware_accelerated,
          jint native_web_contents,
          jint native_window) {
  ContentViewCoreImpl* view = new ContentViewCoreImpl(
      env, obj, input_events_delivered_at_vsync, hardware_accelerated,
      reinterpret_cast<WebContents*>(native_web_contents),
      reinterpret_cast<ui::WindowAndroid*>(native_window));
  return reinterpret_cast<jint>(view);
}

bool RegisterContentViewCore(JNIEnv* env) {
  if (!base::android::HasClass(env, kContentViewCoreClassPath)) {
    DLOG(ERROR) << "Unable to find class ContentViewCore!";
    return false;
  }
  ScopedJavaLocalRef<jclass> clazz = GetClass(env, kContentViewCoreClassPath);
  if (!HasField(env, clazz, "mNativeContentViewCore", "I")) {
    DLOG(ERROR) << "Unable to find ContentView.mNativeContentViewCore!";
    return false;
  }
  g_native_content_view = GetFieldID(env, clazz, "mNativeContentViewCore", "I");

  return RegisterNativesImpl(env) >= 0;
}

}  // namespace content
