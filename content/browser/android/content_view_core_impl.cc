// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_core_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/android/content_view_client.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/navigation_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "jni/content_view_core_jni.h"
#include "webkit/glue/webmenuitem.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::HasField;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

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
  jweak obj;

  ScopedJavaLocalRef<jobject> View(JNIEnv* env) {
    return GetRealObject(env, obj);
  }
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
    : web_contents_(web_contents),
      tab_crashed_(false) {
  DCHECK(web_contents) <<
      "A ContentViewCoreImpl should be created with a valid WebContents.";

  InitJNI(env, obj);
}

ContentViewCoreImpl::~ContentViewCoreImpl() {
  if (java_object_) {
    JNIEnv* env = AttachCurrentThread();
    env->DeleteWeakGlobalRef(java_object_->obj);
    delete java_object_;
    java_object_ = 0;
  }
}

void ContentViewCoreImpl::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void ContentViewCoreImpl::Observe(int type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  // TODO(jrg)
}

void ContentViewCoreImpl::InitJNI(JNIEnv* env, jobject obj) {
  java_object_ = new JavaObject;
  java_object_->obj = env->NewWeakGlobalRef(obj);
}

// ----------------------------------------------------------------------------
// Methods called from Java via JNI
// ----------------------------------------------------------------------------

void ContentViewCoreImpl::LoadUrlWithoutUrlSanitization(JNIEnv* env,
                                                        jobject,
                                                        jstring jurl,
                                                        int page_transition) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));

  LoadUrl(url, page_transition);
}

void ContentViewCoreImpl::LoadUrlWithoutUrlSanitizationWithUserAgentOverride(
    JNIEnv* env,
    jobject,
    jstring jurl,
    int page_transition,
    jstring user_agent_override) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));

  LoadUrlWithUserAgentOverride(
      url,
      page_transition,
      base::android::ConvertJavaStringToUTF8(env, user_agent_override));
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

// --------------------------------------------------------------------------
// Methods called from native code
// --------------------------------------------------------------------------

void ContentViewCoreImpl::LoadUrl(const GURL& url, int page_transition) {
  content::Referrer referer;

  web_contents()->GetController().LoadURL(
      url, referer, content::PageTransitionFromInt(page_transition),
      std::string());
  PostLoadUrl(url);
}

void ContentViewCoreImpl::LoadUrlWithUserAgentOverride(
    const GURL& url,
    int page_transition,
    const std::string& user_agent_override) {
  web_contents()->SetUserAgentOverride(user_agent_override);
  bool is_overriding_user_agent(!user_agent_override.empty());
  content::Referrer referer;
  web_contents()->GetController().LoadURLWithUserAgentOverride(
      url, referer, content::PageTransitionFromInt(page_transition),
      false, std::string(), is_overriding_user_agent);
  PostLoadUrl(url);
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
  Java_ContentViewCore_showSelectPopup(env, java_object_->View(env).obj(),
                                       items_array.obj(), enabled_array.obj(),
                                       multiple, selected_array.obj());
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
  Java_ContentViewCore_didStartLoading(env, java_object_->View(env).obj());
}

void ContentViewCoreImpl::OnAcceleratedCompositingStateChange(
    RenderWidgetHostViewAndroid* rwhva, bool activated, bool force) {
  NOTIMPLEMENTED() << "not upstreamed yet";
}

void ContentViewCoreImpl::StartContentIntent(const GURL& content_url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jcontent_url =
      ConvertUTF8ToJavaString(env, content_url.spec());
  Java_ContentViewCore_startContentIntent(env,
                                          java_object_->View(env).obj(),
                                          jcontent_url.obj());
}

// --------------------------------------------------------------------------
// Methods called from Java via JNI
// --------------------------------------------------------------------------

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
