// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_core_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/json/json_writer.h"
#include "content/browser/android/content_view_client.h"
#include "content/browser/android/load_url_params.h"
#include "content/browser/android/touch_point.h"
#include "content/browser/renderer_host/java/java_bound_object.h"
#include "content/browser/renderer_host/java/java_bridge_dispatcher_host_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/navigation_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "jni/ContentViewCore_jni.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/android/WebInputEventFactory.h"
#include "ui/gfx/android/java_bitmap.h"
#include "webkit/glue/webmenuitem.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::HasField;
using base::android::JavaByteArrayToByteVector;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using WebKit::WebInputEvent;
using WebKit::WebInputEventFactory;

// Describes the type and enabled state of a select popup item.
// Keep in sync with the value defined in SelectPopupDialog.java
enum PopupItemType {
  POPUP_ITEM_TYPE_GROUP = 0,
  POPUP_ITEM_TYPE_DISABLED,
  POPUP_ITEM_TYPE_ENABLED
};

namespace {
jfieldID g_native_content_view;
}  // namespace

namespace content {

struct ContentViewCoreImpl::JavaObject {

};

// ----------------------------------------------------------------------------
// Implementation of static ContentViewCore public interfaces

ContentViewCore* ContentViewCore::Create(JNIEnv* env, jobject obj,
                                         WebContents* web_contents) {
  return new ContentViewCoreImpl(env, obj, web_contents);
}

ContentViewCore* ContentViewCore::GetNativeContentViewCore(JNIEnv* env,
                                                           jobject obj) {
  return reinterpret_cast<ContentViewCore*>(
      env->GetIntField(obj, g_native_content_view));
}

// ----------------------------------------------------------------------------

ContentViewCoreImpl::ContentViewCoreImpl(JNIEnv* env, jobject obj,
                                         WebContents* web_contents)
    : java_ref_(env, obj),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)),
      tab_crashed_(false) {
  DCHECK(web_contents) <<
      "A ContentViewCoreImpl should be created with a valid WebContents.";

  InitJNI(env, obj);

  notification_registrar_.Add(this,
                              NOTIFICATION_EXECUTE_JAVASCRIPT_RESULT,
                              NotificationService::AllSources());
}

ContentViewCoreImpl::~ContentViewCoreImpl() {
  // Make sure nobody calls back into this object while we are tearing things
  // down.
  notification_registrar_.RemoveAll();

  delete java_object_;
  java_object_ = NULL;
  java_ref_.reset();
}

void ContentViewCoreImpl::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void ContentViewCoreImpl::Observe(int type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_EXECUTE_JAVASCRIPT_RESULT: {
      if (!web_contents_ || Source<RenderViewHost>(source).ptr() !=
          web_contents_->GetRenderViewHost()) {
        return;
      }

      JNIEnv* env = base::android::AttachCurrentThread();
      std::pair<int, Value*>* result_pair =
          Details<std::pair<int, Value*> >(details).ptr();
      std::string json;
      base::JSONWriter::Write(result_pair->second, &json);
      ScopedJavaLocalRef<jstring> j_json = ConvertUTF8ToJavaString(env, json);
      ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
      if (!j_obj.is_null()) {
        Java_ContentViewCore_onEvaluateJavaScriptResult(env, j_obj.obj(),
            static_cast<jint>(result_pair->first), j_json.obj());
      }
      break;
    }
  }

  // TODO(jrg)
}

void ContentViewCoreImpl::InitJNI(JNIEnv* env, jobject obj) {
  java_object_ = new JavaObject;
}

RenderWidgetHostViewAndroid*
    ContentViewCoreImpl::GetRenderWidgetHostViewAndroid() {
  RenderWidgetHostView* rwhv = NULL;
  if (web_contents_)
    rwhv = web_contents_->GetRenderWidgetHostView();
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
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

jint ContentViewCoreImpl::EvaluateJavaScript(JNIEnv* env, jobject obj,
                                             jstring script) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();

  string16 script_utf16 = ConvertJavaStringToUTF16(env, script);
  return host->ExecuteJavascriptInWebFrameNotifyResult(string16(),
                                                       script_utf16);
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
    jstring virtual_url_for_data_url) {
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

  LoadUrl(params);
}

ScopedJavaLocalRef<jstring> ContentViewCoreImpl::GetURL(
    JNIEnv* env, jobject) const {
  return ConvertUTF8ToJavaString(env, web_contents()->GetURL().spec());
}

ScopedJavaLocalRef<jstring> ContentViewCoreImpl::GetTitle(
    JNIEnv* env, jobject obj) const {
  return ConvertUTF16ToJavaString(env, web_contents()->GetTitle());
}

jdouble ContentViewCoreImpl::GetLoadProgress(JNIEnv* env, jobject obj) const {
  // An empty page never loads anything and always has a progress of 0.
  // We report 1 in that case so the UI does not assume the page is loading.
  if (web_contents()->GetURL().is_empty() || !content_view_client_.get())
    return static_cast<jdouble>(1.0);
  return static_cast<jdouble>(content_view_client_->GetLoadProgress());
}

jboolean ContentViewCoreImpl::IsIncognito(JNIEnv* env, jobject obj) {
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

jboolean ContentViewCoreImpl::TouchEvent(JNIEnv* env,
                                         jobject obj,
                                         jlong time_ms,
                                         jint type,
                                         jobjectArray pts) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv) {
    using WebKit::WebTouchEvent;
    WebKit::WebTouchEvent event;
    TouchPoint::BuildWebTouchEvent(env, type, time_ms, pts, event);
    rwhv->TouchEvent(event);
    return true;
  }
  return false;
}

void ContentViewCoreImpl::SendGestureEvent(WebInputEvent::Type type,
                                           long time_ms, int x, int y,
                                           float dx, float dy,
                                           bool link_preview_tap) {
  WebKit::WebGestureEvent event = WebInputEventFactory::gestureEvent(
      type, time_ms / 1000.0, x, y, dx, dy, 0);
  if (GetRenderWidgetHostViewAndroid())
    GetRenderWidgetHostViewAndroid()->GestureEvent(event);
}

void ContentViewCoreImpl::ScrollBegin(JNIEnv* env, jobject obj, jlong time_ms,
                                      jint x, jint y) {
  SendGestureEvent(
      WebInputEvent::GestureScrollBegin, time_ms, x, y, 0, 0, false);
}

void ContentViewCoreImpl::ScrollEnd(JNIEnv* env, jobject obj, jlong time_ms) {
  SendGestureEvent(WebInputEvent::GestureScrollEnd, time_ms, 0, 0, 0, 0, false);
}

void ContentViewCoreImpl::ScrollBy(JNIEnv* env, jobject obj, jlong time_ms,
                                   jint dx, jint dy) {
  SendGestureEvent(
      WebInputEvent::GestureScrollUpdate, time_ms, 0, 0, -dx, -dy, false);
}

void ContentViewCoreImpl::FlingStart(JNIEnv* env, jobject obj, jlong time_ms,
                                     jint x, jint y, jint vx, jint vy) {
  SendGestureEvent(
      WebInputEvent::GestureFlingStart, time_ms, x, y, vx, vy, false);
}

void ContentViewCoreImpl::FlingCancel(JNIEnv* env, jobject obj, jlong time_ms) {
  SendGestureEvent(
      WebInputEvent::GestureFlingCancel, time_ms, 0, 0, 0, 0, false);
}

void ContentViewCoreImpl::SingleTap(JNIEnv* env, jobject obj, jlong time_ms,
                                    jint x, jint y, jboolean link_preview_tap) {
  SendGestureEvent(
      WebInputEvent::GestureTap, time_ms, x, y, 0, 0, link_preview_tap);
}

void ContentViewCoreImpl::ShowPressState(JNIEnv* env, jobject obj,
                                         jlong time_ms,
                                         jint x, jint y) {
  SendGestureEvent(WebInputEvent::GestureTapDown, time_ms, x, y, 0, 0, false);
}

void ContentViewCoreImpl::DoubleTap(JNIEnv* env, jobject obj, jlong time_ms,
                                    jint x, jint y) {
  SendGestureEvent(WebInputEvent::GestureDoubleTap, time_ms, x, y, 0, 0, false);
}

void ContentViewCoreImpl::LongPress(JNIEnv* env, jobject obj, jlong time_ms,
                                    jint x, jint y, jboolean link_preview_tap) {
  SendGestureEvent(
      WebInputEvent::GestureLongPress, time_ms, x, y, 0, 0, link_preview_tap);
}

void ContentViewCoreImpl::PinchBegin(JNIEnv* env, jobject obj, jlong time_ms,
                                     jint x, jint y) {
  SendGestureEvent(
      WebInputEvent::GesturePinchBegin, time_ms, x, y, 0, 0, false);
}

void ContentViewCoreImpl::PinchEnd(JNIEnv* env, jobject obj, jlong time_ms) {
  SendGestureEvent(WebInputEvent::GesturePinchEnd, time_ms, 0, 0, 0, 0, false);
}

void ContentViewCoreImpl::PinchBy(JNIEnv* env, jobject obj, jlong time_ms,
                                  jint anchor_x, jint anchor_y, jfloat delta) {
  SendGestureEvent(WebInputEvent::GesturePinchUpdate,
                   time_ms,
                   anchor_x,
                   anchor_y,
                   delta,
                   delta,
                   false);
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

void ContentViewCoreImpl::StopLoading(JNIEnv* env, jobject obj) {
  web_contents_->Stop();
}

void ContentViewCoreImpl::Reload(JNIEnv* env, jobject obj) {
  // Set check_for_repost parameter to false as we have no repost confirmation
  // dialog ("confirm form resubmission" screen will still appear, however).
  web_contents_->GetController().Reload(false);
  tab_crashed_ = false;
}

void ContentViewCoreImpl::ClearHistory(JNIEnv* env, jobject obj) {
  web_contents_->GetController().PruneAllButActive();
}

jboolean ContentViewCoreImpl::NeedsReload(JNIEnv* env, jobject obj) {
  return web_contents_->GetController().NeedsReload();
}

void ContentViewCoreImpl::SetClient(JNIEnv* env, jobject obj, jobject jclient) {
  scoped_ptr<ContentViewClient> client(
      ContentViewClient::CreateNativeContentViewClient(env, jclient));

  web_contents_->SetDelegate(client.get());

  content_view_client_.swap(client);
}

void ContentViewCoreImpl::AddJavascriptInterface(
    JNIEnv* env,
    jobject /* obj */,
    jobject object,
    jstring name,
    jboolean require_annotation) {
  ScopedJavaLocalRef<jobject> scoped_object(env, object);
  // JavaBoundObject creates the NPObject with a ref count of 1, and
  // JavaBridgeDispatcherHostManager takes its own ref.
  NPObject* bound_object = JavaBoundObject::Create(scoped_object,
                                                   require_annotation);
  web_contents_->java_bridge_dispatcher_host_manager()->AddNamedObject(
      ConvertJavaStringToUTF16(env, name), bound_object);
  WebKit::WebBindings::releaseObject(bound_object);
}

void ContentViewCoreImpl::RemoveJavascriptInterface(JNIEnv* env,
                                                    jobject /* obj */,
                                                    jstring name) {
  web_contents_->java_bridge_dispatcher_host_manager()->RemoveNamedObject(
      ConvertJavaStringToUTF16(env, name));
}

int ContentViewCoreImpl::GetNavigationHistory(JNIEnv* env,
                                              jobject obj,
                                              jobject context) {
  // Iterate through navigation entries to populate the list
  const NavigationController& controller = web_contents_->GetController();
  int count = controller.GetEntryCount();
  for (int i = 0; i < count; ++i) {
    NavigationEntry* entry = controller.GetEntryAtIndex(i);

    // Get the details of the current entry
    ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env,
        entry->GetURL().spec());
    ScopedJavaLocalRef<jstring> j_virtual_url = ConvertUTF8ToJavaString(env,
        entry->GetVirtualURL().spec());
    ScopedJavaLocalRef<jstring> j_original_url = ConvertUTF8ToJavaString(env,
        entry->GetOriginalRequestURL().spec());
    ScopedJavaLocalRef<jstring> j_title = ConvertUTF16ToJavaString(env,
        entry->GetTitle());
    ScopedJavaLocalRef<jobject> j_bitmap;
    const FaviconStatus& status = entry->GetFavicon();
    if (status.valid && status.image.ToSkBitmap()->getSize() > 0) {
      j_bitmap = gfx::ConvertToJavaBitmap(status.image.ToSkBitmap());
    }

    // Add the item to the list
    Java_ContentViewCore_addToNavigationHistory(env, obj, context, j_url.obj(),
        j_virtual_url.obj(), j_original_url.obj(), j_title.obj(),
        j_bitmap.obj());
  }

  return controller.GetCurrentEntryIndex();
}

// --------------------------------------------------------------------------
// Methods called from native code
// --------------------------------------------------------------------------

void ContentViewCoreImpl::LoadUrl(
    NavigationController::LoadURLParams& params) {
  web_contents()->GetController().LoadURLWithParams(params);
  PostLoadUrl(params.url);
}

void ContentViewCoreImpl::PostLoadUrl(const GURL& url) {
  tab_crashed_ = false;
  // TODO(tedchoc): Update the content view client of the page load request.
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

// This is called for each ContentViewCore.
jint Init(JNIEnv* env, jobject obj, jint native_web_contents) {
  ContentViewCore* view = ContentViewCore::Create(
      env, obj, reinterpret_cast<WebContents*>(native_web_contents));
  return reinterpret_cast<jint>(view);
}

jint EvaluateJavaScript(JNIEnv* env, jobject obj, jstring script) {
  ContentViewCoreImpl* view = static_cast<ContentViewCoreImpl*>
      (ContentViewCore::GetNativeContentViewCore(env, obj));

  return view->EvaluateJavaScript(env, obj, script);
}

// --------------------------------------------------------------------------
// Public methods that call to Java via JNI
// --------------------------------------------------------------------------

void ContentViewCoreImpl::OnTabCrashed(const base::ProcessHandle handle) {
  NOTIMPLEMENTED() << "not upstreamed yet";
}

void ContentViewCoreImpl::SetTitle(const string16& title) {
  NOTIMPLEMENTED() << "not upstreamed yet";
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

void ContentViewCoreImpl::ConfirmTouchEvent(bool handled) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_ContentViewCore_confirmTouchEvent(env, j_obj.obj(), handled);
}

void ContentViewCoreImpl::DidSetNeedTouchEvents(bool need_touch_events) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_ContentViewCore_didSetNeedTouchEvents(env,
                                             j_obj.obj(),
                                             need_touch_events);
}

bool ContentViewCoreImpl::HasFocus() {
  NOTIMPLEMENTED() << "not upstreamed yet";
  return false;
}

void ContentViewCoreImpl::OnSelectionChanged(const std::string& text) {
  NOTIMPLEMENTED() << "not upstreamed yet";
}

void ContentViewCoreImpl::OnSelectionBoundsChanged(
    int startx,
    int starty,
    base::i18n::TextDirection start_dir,
    int endx,
    int endy,
    base::i18n::TextDirection end_dir) {
  NOTIMPLEMENTED() << "not upstreamed yet";
}

void ContentViewCoreImpl::DidStartLoading() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_ContentViewCore_didStartLoading(env, j_obj.obj());
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

// --------------------------------------------------------------------------
// Methods called from native code
// --------------------------------------------------------------------------

gfx::Rect ContentViewCoreImpl::GetBounds() const {
  NOTIMPLEMENTED() << "not upstreamed yet";
  return gfx::Rect();
}

// ----------------------------------------------------------------------------

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
