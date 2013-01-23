// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/intercept_navigation_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "components/navigation_interception/intercept_navigation_resource_throttle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "jni/InterceptNavigationDelegate_jni.h"
#include "net/url_request/url_request.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using content::PageTransition;
using content::RenderViewHost;
using content::WebContents;

namespace components {

namespace {

const void* kInterceptNavigationDelegateUserDataKey =
    &kInterceptNavigationDelegateUserDataKey;

bool CheckIfShouldIgnoreNavigationOnUIThread(RenderViewHost* source,
                                             const GURL& url,
                                             const content::Referrer& referrer,
                                             bool is_post,
                                             bool has_user_gesture,
                                             PageTransition transition_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(source);

  WebContents* web_contents = WebContents::FromRenderViewHost(source);
  if (!web_contents)
    return false;
  InterceptNavigationDelegate* intercept_navigation_delegate =
      InterceptNavigationDelegate::Get(web_contents);
  if (!intercept_navigation_delegate)
    return false;

  return intercept_navigation_delegate->ShouldIgnoreNavigation(
      url, is_post, has_user_gesture, transition_type);
}

} // namespace

// static
void InterceptNavigationDelegate::Associate(
    WebContents* web_contents,
    scoped_ptr<InterceptNavigationDelegate> delegate) {
  web_contents->SetUserData(kInterceptNavigationDelegateUserDataKey,
                            delegate.release());
}

// static
InterceptNavigationDelegate* InterceptNavigationDelegate::Get(
    WebContents* web_contents) {
  return reinterpret_cast<InterceptNavigationDelegate*>(
      web_contents->GetUserData(kInterceptNavigationDelegateUserDataKey));
}

// static
content::ResourceThrottle* InterceptNavigationDelegate::CreateThrottleFor(
    net::URLRequest* request) {
  return new InterceptNavigationResourceThrottle(
      request, base::Bind(&CheckIfShouldIgnoreNavigationOnUIThread));
}

InterceptNavigationDelegate::InterceptNavigationDelegate(
    JNIEnv* env, jobject jdelegate)
    : weak_jdelegate_(env, jdelegate) {
}

InterceptNavigationDelegate::~InterceptNavigationDelegate() {
}

bool InterceptNavigationDelegate::ShouldIgnoreNavigation(
    const GURL& url,
    bool is_post,
    bool has_user_gesture,
    PageTransition transition_type) {
  if (!url.is_valid())
    return false;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = weak_jdelegate_.get(env);

  if (jdelegate.is_null())
    return false;

  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());
  return Java_InterceptNavigationDelegate_shouldIgnoreNavigation(
      env,
      jdelegate.obj(),
      jstring_url.obj(),
      is_post,
      has_user_gesture,
      transition_type);
}

// Register native methods.

bool RegisterInterceptNavigationDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace components
