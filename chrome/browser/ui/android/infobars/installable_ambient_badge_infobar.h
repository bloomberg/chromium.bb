// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_INSTALLABLE_AMBIENT_BADGE_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_INSTALLABLE_AMBIENT_BADGE_INFOBAR_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"

class InstallableAmbientBadgeInfoBarDelegate;

// An infobar shown to users when they visit a progressive web app.
class InstallableAmbientBadgeInfoBar : public InfoBarAndroid {
 public:
  explicit InstallableAmbientBadgeInfoBar(
      std::unique_ptr<InstallableAmbientBadgeInfoBarDelegate> delegate);
  ~InstallableAmbientBadgeInfoBar() override;

  void AddToHomescreen(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

 private:
  InstallableAmbientBadgeInfoBarDelegate* GetDelegate();

  // InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;
  void ProcessButton(int action) override;

  DISALLOW_COPY_AND_ASSIGN(InstallableAmbientBadgeInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_INSTALLABLE_AMBIENT_BADGE_INFOBAR_H_
