// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_WEB_CONTENTS_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_CHROME_WEB_CONTENTS_DELEGATE_ANDROID_H_

#include <jni.h>

#include "chrome/browser/component/web_contents_delegate_android/web_contents_delegate_android.h"

namespace content {
struct FileChooserParams;
class WebContents;
}

namespace chrome {
namespace android {

// Chromium Android specific WebContentsDelegate.
// Should contain any WebContentsDelegate implementations required by
// the Chromium Android port but not to be shared with WebView.
class ChromeWebContentsDelegateAndroid
    : public web_contents_delegate_android::WebContentsDelegateAndroid {
 public:
  ChromeWebContentsDelegateAndroid(JNIEnv* env, jobject obj);
  virtual ~ChromeWebContentsDelegateAndroid();

  virtual void RunFileChooser(content::WebContents* web_contents,
                              const content::FileChooserParams& params)
                              OVERRIDE;
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_CHROME_WEB_CONTENTS_DELEGATE_ANDROID_H_
