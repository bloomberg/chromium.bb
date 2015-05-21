// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_BACKGROUND_CONTENT_VIEW_HELPER_H_
#define CHROME_BROWSER_ANDROID_TAB_BACKGROUND_CONTENT_VIEW_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/layers/layer.h"

namespace content {
class WebContents;
}

namespace chrome {
namespace android {
class ChromeWebContentsDelegateAndroid;
}
}

class BackgroundContentViewHelper {
 public:
  static BackgroundContentViewHelper* FromJavaObject(JNIEnv* env,
                                                     jobject jobj);
  static bool Register(JNIEnv* env);

  BackgroundContentViewHelper();
  virtual ~BackgroundContentViewHelper();

  void Destroy(JNIEnv* env, jobject jobj);
  void SetWebContents(JNIEnv* env,
                      jobject jobj,
                      jobject jcontent_view_core,
                      jobject jweb_contents_delegate);
  void DestroyWebContents(JNIEnv* env, jobject jobj);
  void ReleaseWebContents(JNIEnv* env, jobject jobj);
  void MergeHistoryFrom(JNIEnv* env, jobject jobj, jobject jweb_contents);
  // Calls 'unload' handler and destroys the web_contents passed in.
  void UnloadAndDeleteWebContents(JNIEnv* env,
                                  jobject jobj,
                                  jobject jweb_contents);
  int GetSwapTimeoutMs(JNIEnv* env, jobject obj);
  int GetNumFramesNeededForSwap(JNIEnv* env, jobject obj);

 private:
  scoped_refptr<cc::Layer> GetContentLayer();

  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<chrome::android::ChromeWebContentsDelegateAndroid>
      web_contents_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundContentViewHelper);
};

#endif  // CHROME_BROWSER_ANDROID_TAB_BACKGROUND_CONTENT_VIEW_HELPER_H_
