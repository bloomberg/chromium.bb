// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/intercept_navigation_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "components/navigation_interception/intercept_navigation_resource_throttle.h"
#include "components/navigation_interception/navigation_params_android.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/InterceptNavigationDelegate_jni.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using ui::PageTransition;
using content::RenderViewHost;
using content::WebContents;

namespace navigation_interception {

namespace {

const void* kInterceptNavigationDelegateUserDataKey =
    &kInterceptNavigationDelegateUserDataKey;

bool CheckIfShouldIgnoreNavigationOnUIThread(WebContents* source,
                                             const NavigationParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(source);

  InterceptNavigationDelegate* intercept_navigation_delegate =
      InterceptNavigationDelegate::Get(source);
  if (!intercept_navigation_delegate)
    return false;

  return intercept_navigation_delegate->ShouldIgnoreNavigation(params);
}

}  // namespace

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
    const NavigationParams& navigation_params) {
  if (!navigation_params.url().is_valid())
    return false;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = weak_jdelegate_.get(env);

  if (jdelegate.is_null())
    return false;

  ScopedJavaLocalRef<jobject> jobject_params =
      CreateJavaNavigationParams(env, navigation_params);

  return Java_InterceptNavigationDelegate_shouldIgnoreNavigation(
      env,
      jdelegate.obj(),
      jobject_params.obj());
}

// Register native methods.

bool RegisterInterceptNavigationDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace navigation_interception
