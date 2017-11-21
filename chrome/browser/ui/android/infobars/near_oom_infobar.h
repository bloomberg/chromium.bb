// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_NEAR_OOM_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_NEAR_OOM_INFOBAR_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"

namespace content {
class WebContents;
}

class InterventionDelegate;

// Communicates to the user about the intervention performed by the browser to
// limit the page's memory usage. See NearOomInfoBar.java for UI specifics, and
// NearOomMessageDelegate for behavior specifics.
class NearOomInfoBar : public InfoBarAndroid {
 public:
  ~NearOomInfoBar() override;

  // |delegate| must remain alive while showing this info bar.
  static void Show(content::WebContents* web_contents,
                   InterventionDelegate* delegate);

 private:
  explicit NearOomInfoBar(InterventionDelegate* delegate);

  // InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;
  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj) override;
  void ProcessButton(int action) override;

  InterventionDelegate* delegate_;
  DISALLOW_COPY_AND_ASSIGN(NearOomInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_NEAR_OOM_INFOBAR_H_
