// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feedback/connectivity_checker.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/ConnectivityChecker_jni.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace chrome {
namespace android {

namespace {

void ExecuteCallback(jobject callback, bool connected) {
  Java_ConnectivityChecker_executeCallback(base::android::AttachCurrentThread(),
                                           callback, connected);
}

void ExecuteCallbackFromRef(
    base::android::ScopedJavaGlobalRef<jobject>* callback,
    bool connected) {
  ExecuteCallback(callback->obj(), connected);
}

void PostCallback(JNIEnv* env, jobject j_callback, bool connected) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ExecuteCallbackFromRef,
                 base::Owned(new base::android::ScopedJavaGlobalRef<jobject>(
                     env, j_callback)),
                 connected));
}

// A utility class for checking if the device is currently connected to the
// Internet.
class ConnectivityChecker : public net::URLFetcherDelegate {
 public:
  ConnectivityChecker(Profile* profile,
                      const GURL& url,
                      const base::TimeDelta& timeout,
                      const base::android::JavaRef<jobject>& java_callback);

  // Kicks off the asynchronous connectivity check. When the request has
  // completed, |this| is deleted.
  void StartAsyncCheck();

  // net::URLFetcherDelegate implementation:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Cancels the URLFetcher, and triggers the callback with a negative result.
  void Cancel();

 private:
  // The context in which the connectivity check is performed.
  net::URLRequestContextGetter* request_context_;

  // The URL to connect to.
  const GURL& url_;

  // How long to wait for a response before giving up.
  const base::TimeDelta timeout_;

  // Holds the Java object which will get the callback with the result.
  base::android::ScopedJavaGlobalRef<jobject> java_callback_;

  // The URLFetcher that executes the connectivity check.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  // Whether |this| is already being destroyed, at which point the callback
  // has already happened, and no further action should be taken.
  bool is_being_destroyed_;

  scoped_ptr<base::OneShotTimer<ConnectivityChecker>> expiration_timer_;
};

void ConnectivityChecker::OnURLFetchComplete(const net::URLFetcher* source) {
  if (is_being_destroyed_)
    return;
  is_being_destroyed_ = true;

  DCHECK_EQ(url_fetcher_.get(), source);
  net::URLRequestStatus status = source->GetStatus();
  int response_code = source->GetResponseCode();

  bool connected = status.is_success() && response_code == net::HTTP_NO_CONTENT;
  ExecuteCallback(java_callback_.obj(), connected);

  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

ConnectivityChecker::ConnectivityChecker(
    Profile* profile,
    const GURL& url,
    const base::TimeDelta& timeout,
    const base::android::JavaRef<jobject>& java_callback)
    : request_context_(profile->GetRequestContext()),
      url_(url),
      timeout_(timeout),
      java_callback_(java_callback),
      is_being_destroyed_(false) {
}

void ConnectivityChecker::StartAsyncCheck() {
  url_fetcher_ =
      net::URLFetcher::Create(url_, net::URLFetcher::GET, this).Pass();
  url_fetcher_->SetRequestContext(request_context_);
  url_fetcher_->SetStopOnRedirect(true);
  url_fetcher_->SetAutomaticallyRetryOn5xx(false);
  url_fetcher_->SetAutomaticallyRetryOnNetworkChanges(0);
  url_fetcher_->SetLoadFlags(net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
                             net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SEND_AUTH_DATA);
  url_fetcher_->Start();
  expiration_timer_.reset(new base::OneShotTimer<ConnectivityChecker>());
  expiration_timer_->Start(FROM_HERE, timeout_, this,
                           &ConnectivityChecker::Cancel);
}

void ConnectivityChecker::Cancel() {
  if (is_being_destroyed_)
    return;
  is_being_destroyed_ = true;
  url_fetcher_.reset();
  ExecuteCallback(java_callback_.obj(), false);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace

void CheckConnectivity(JNIEnv* env,
                       jclass clazz,
                       jobject j_profile,
                       jstring j_url,
                       jlong j_timeout_ms,
                       jobject j_callback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  if (!profile) {
    PostCallback(env, j_callback, false);
    return;
  }
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  if (!url.is_valid()) {
    PostCallback(env, j_callback, false);
    return;
  }

  // This object will be deleted when the connectivity check has completed.
  ConnectivityChecker* connectivity_checker = new ConnectivityChecker(
      profile, url, base::TimeDelta::FromMilliseconds(j_timeout_ms),
      base::android::ScopedJavaLocalRef<jobject>(env, j_callback));
  connectivity_checker->StartAsyncCheck();
}

bool RegisterConnectivityChecker(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
