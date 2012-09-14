// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_HTTP_AUTH_HANDLER_H_
#define ANDROID_WEBVIEW_NATIVE_AW_HTTP_AUTH_HANDLER_H_

#include <jni.h>
#include <string>

#include "android_webview/browser/aw_login_delegate.h"
#include "android_webview/browser/aw_http_auth_handler_base.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"

namespace content {
class WebContents;
};

namespace net {
class AuthChallengeInfo;
};

namespace android_webview {

// Native class for Java class of same name and owns an instance
// of that Java object.
// One instance of this class is created per underlying AwLoginDelegate.
class AwHttpAuthHandler : public AwHttpAuthHandlerBase {
 public:
  AwHttpAuthHandler(AwLoginDelegate* login_delegate,
                    net::AuthChallengeInfo* auth_info);
  virtual ~AwHttpAuthHandler();

  // from AwHttpAuthHandler
  virtual void HandleOnUIThread(content::WebContents* web_contents) OVERRIDE;

  void Proceed(JNIEnv* env, jobject obj, jstring username, jstring password);
  void Cancel(JNIEnv* env, jobject obj);

 private:
  scoped_refptr<AwLoginDelegate> login_delegate_;
  base::android::ScopedJavaGlobalRef<jobject> http_auth_handler_;
  std::string host_;
  std::string realm_;
};

bool RegisterAwHttpAuthHandler(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_HTTP_AUTH_HANDLER_H_
