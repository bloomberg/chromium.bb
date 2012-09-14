// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_
#define ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/android/jni_helper.h"
#include "base/memory/scoped_ptr.h"

class TabContents;

namespace content {
class WebContents;
}

namespace android_webview {

class AwContentsContainer;
class AwRenderViewHostExt;
class AwWebContentsDelegate;

// Native side of java-class of same name.
// Provides the ownership of and access to browser components required for
// WebView functionality; analogous to chrome's TabContents, but with a
// level of indirection provided by the AwContentsContainer abstraction.
class AwContents {
 public:
  // Returns the AwContents instance associated with |web_contents|, or NULL.
  static AwContents* FromWebContents(content::WebContents* web_contents);

  AwContents(JNIEnv* env,
             jobject obj,
             jobject web_contents_delegate,
             bool private_browsing);
  ~AwContents();

  // |handler| is an instance of
  // org.chromium.android_webview.AwHttpAuthHandler.
  void onReceivedHttpAuthRequest(const base::android::JavaRef<jobject>& handler,
                                 const std::string& host,
                                 const std::string& realm);

  // Methods called from Java.
  jint GetWebContents(JNIEnv* env, jobject obj);
  void Destroy(JNIEnv* env, jobject obj);
  void DocumentHasImages(JNIEnv* env, jobject obj, jobject message);

 private:
  JavaObjectWeakGlobalRef java_ref_;
  scoped_ptr<AwContentsContainer> contents_container_;
  scoped_ptr<AwWebContentsDelegate> web_contents_delegate_;
  scoped_ptr<AwRenderViewHostExt> render_view_host_ext_;

  DISALLOW_COPY_AND_ASSIGN(AwContents);
};

bool RegisterAwContents(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_
