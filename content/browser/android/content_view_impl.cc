// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/web_contents/navigation_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "jni/content_view_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::HasField;

namespace {
jfieldID g_native_content_view;
}  // namespace

namespace content {

// ----------------------------------------------------------------------------
// Implementation of static ContentView public interfaces

ContentView* ContentView::Create(JNIEnv* env, jobject obj,
                                 WebContents* web_contents) {
  return new ContentViewImpl(env, obj, web_contents);
}

ContentView* ContentView::GetNativeContentView(JNIEnv* env, jobject obj) {
  return reinterpret_cast<ContentView*>(
      env->GetIntField(obj, g_native_content_view));
}

// ----------------------------------------------------------------------------

ContentViewImpl::ContentViewImpl(JNIEnv* env, jobject obj,
                                 WebContents* web_contents)
    : web_contents_(web_contents),
      tab_crashed_(false) {
  DCHECK(web_contents) <<
      "A ContentViewImpl should be created with a valid WebContents.";
}

ContentViewImpl::~ContentViewImpl() {
}

void ContentViewImpl::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void ContentViewImpl::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  // TODO(jrg)
}

// ----------------------------------------------------------------------------
// Methods called from Java via JNI
// ----------------------------------------------------------------------------

void ContentViewImpl::LoadUrlWithoutUrlSanitization(JNIEnv* env,
                                                    jobject,
                                                    jstring jurl,
                                                    int page_transition) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));

  LoadUrl(url, page_transition);
}

void ContentViewImpl::LoadUrlWithoutUrlSanitizationWithUserAgentOverride(
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

ScopedJavaLocalRef<jstring> ContentViewImpl::GetURL(
    JNIEnv* env, jobject) const {
  return ConvertUTF8ToJavaString(env, web_contents()->GetURL().spec());
}

ScopedJavaLocalRef<jstring> ContentViewImpl::GetTitle(
    JNIEnv* env, jobject obj) const {
  return ConvertUTF16ToJavaString(env, web_contents()->GetTitle());
}

jdouble ContentViewImpl::GetLoadProgress(JNIEnv* env, jobject obj) const {
  // An empty page never loads anything and always has a progress of 0.
  // We report 1 in that case so the UI does not assume the page is loading.
  if (web_contents()->GetURL().is_empty())
    return static_cast<jdouble>(1.0);
  // TODO(tedchoc): Add GetLoadProgress() to web_contents.
  return static_cast<jdouble>(0.0);
}

jboolean ContentViewImpl::IsIncognito(JNIEnv* env, jobject obj) {
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

jboolean ContentViewImpl::CanGoBack(JNIEnv* env, jobject obj) {
  return web_contents_->GetController().CanGoBack();
}

jboolean ContentViewImpl::CanGoForward(JNIEnv* env, jobject obj) {
  return web_contents_->GetController().CanGoForward();
}

jboolean ContentViewImpl::CanGoToOffset(
    JNIEnv* env, jobject obj, jint offset) {
  return web_contents_->GetController().CanGoToOffset(offset);
}

void ContentViewImpl::GoBack(JNIEnv* env, jobject obj) {
  web_contents_->GetController().GoBack();
  tab_crashed_ = false;
}

void ContentViewImpl::GoForward(JNIEnv* env, jobject obj) {
  web_contents_->GetController().GoForward();
  tab_crashed_ = false;
}

void ContentViewImpl::GoToOffset(JNIEnv* env, jobject obj, jint offset) {
  web_contents_->GetController().GoToOffset(offset);
}

void ContentViewImpl::StopLoading(JNIEnv* env, jobject obj) {
  web_contents_->Stop();
}

void ContentViewImpl::Reload(JNIEnv* env, jobject obj) {
  // Set check_for_repost parameter to false as we have no repost confirmation
  // dialog ("confirm form resubmission" screen will still appear, however).
  web_contents_->GetController().Reload(false);
  tab_crashed_ = false;
}

void ContentViewImpl::ClearHistory(JNIEnv* env, jobject obj) {
  web_contents_->GetController().PruneAllButActive();
}

jboolean ContentViewImpl::NeedsReload(JNIEnv* env, jobject obj) {
  return web_contents_->GetController().NeedsReload();
}

// --------------------------------------------------------------------------
// Methods called from native code
// --------------------------------------------------------------------------

void ContentViewImpl::LoadUrl(const GURL& url, int page_transition) {
  content::Referrer referer;

  web_contents()->GetController().LoadURL(
      url, referer, content::PageTransitionFromInt(page_transition),
      std::string());
  PostLoadUrl(url);
}

void ContentViewImpl::LoadUrlWithUserAgentOverride(
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

void ContentViewImpl::PostLoadUrl(const GURL& url) {
  tab_crashed_ = false;
  // TODO(tedchoc): Update the content view client of the page load request.
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

// This is called for each ContentView.
static jint Init(JNIEnv* env, jobject obj, jint native_web_contents) {
  ContentView* view = ContentView::Create(
      env, obj, reinterpret_cast<WebContents*>(native_web_contents));
  return reinterpret_cast<jint>(view);
}

// --------------------------------------------------------------------------
// Public methods that call to Java via JNI
// --------------------------------------------------------------------------

void ContentViewImpl::OnTabCrashed(const base::ProcessHandle handle) {
  NOTIMPLEMENTED() << "not upstreamed yet";
}

void ContentViewImpl::SetTitle(const string16& title) {
  NOTIMPLEMENTED() << "not upstreamed yet";
}

bool ContentViewImpl::HasFocus() {
  NOTIMPLEMENTED() << "not upstreamed yet";
  return false;
}

void ContentViewImpl::OnSelectionChanged(const std::string& text) {
  NOTIMPLEMENTED() << "not upstreamed yet";
}

void ContentViewImpl::OnSelectionBoundsChanged(
    int startx,
    int starty,
    base::i18n::TextDirection start_dir,
    int endx,
    int endy,
    base::i18n::TextDirection end_dir) {
  NOTIMPLEMENTED() << "not upstreamed yet";
}

void ContentViewImpl::OnAcceleratedCompositingStateChange(
    RenderWidgetHostViewAndroid* rwhva, bool activated, bool force) {
  NOTIMPLEMENTED() << "not upstreamed yet";
}

// --------------------------------------------------------------------------
// Methods called from Java via JNI
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Methods called from native code
// --------------------------------------------------------------------------

gfx::Rect ContentViewImpl::GetBounds() const {
  NOTIMPLEMENTED() << "not upstreamed yet";
  return gfx::Rect();
}

// ----------------------------------------------------------------------------

bool RegisterContentView(JNIEnv* env) {
  if (!base::android::HasClass(env, kContentViewClassPath)) {
    DLOG(ERROR) << "Unable to find class ContentView!";
    return false;
  }
  ScopedJavaLocalRef<jclass> clazz = GetClass(env, kContentViewClassPath);
  if (!HasField(env, clazz, "mNativeContentView", "I")) {
    DLOG(ERROR) << "Unable to find ContentView.mNativeContentView!";
    return false;
  }
  g_native_content_view = GetFieldID(env, clazz, "mNativeContentView", "I");

  return RegisterNativesImpl(env) >= 0;
}

}  // namespace content
