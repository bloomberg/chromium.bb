// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_FRAMEBUST_BLOCK_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_FRAMEBUST_BLOCK_INFOBAR_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#include "chrome/browser/ui/android/interventions/framebust_block_message_delegate_bridge.h"

namespace content {
class WebContents;
}

class FramebustBlockMessageDelegate;

// Communicates to the user about the intervention performed by the browser by
// blocking a framebust. See FramebustBlockInfoBar.java for UI specifics, and
// FramebustBlockMessageDelegate for behavior specifics.
class FramebustBlockInfoBar : public InfoBarAndroid {
 public:
  ~FramebustBlockInfoBar() override;

  static void Show(content::WebContents* web_contents,
                   std::unique_ptr<FramebustBlockMessageDelegate> delegate);

 private:
  explicit FramebustBlockInfoBar(
      std::unique_ptr<FramebustBlockMessageDelegate> delegate);

  // InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;
  void ProcessButton(int action) override;

  std::unique_ptr<FramebustBlockMessageDelegateBridge> delegate_bridge_;

  DISALLOW_COPY_AND_ASSIGN(FramebustBlockInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_FRAMEBUST_BLOCK_INFOBAR_H_
