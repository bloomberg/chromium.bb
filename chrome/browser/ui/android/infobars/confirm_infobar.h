// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_CONFIRM_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_CONFIRM_INFOBAR_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class ConfirmInfoBar : public InfoBarAndroid {
 public:
  explicit ConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate> delegate);
  ~ConfirmInfoBar() override;

 protected:
  base::string16 GetTextFor(ConfirmInfoBarDelegate::InfoBarButton button);
  ConfirmInfoBarDelegate* GetDelegate();
  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj) override;
  base::android::ScopedJavaLocalRef<jobject> GetWindowAndroid();

  // InfoBarAndroid overrides.
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;

 private:
  void ProcessButton(int action) override;

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBar);
};

// Registers native methods.
bool RegisterConfirmInfoBar(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_CONFIRM_INFOBAR_H_
