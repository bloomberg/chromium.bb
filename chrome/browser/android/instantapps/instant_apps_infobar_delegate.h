// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_INFOBAR_DELEGATE_H_

#include "base/android/jni_android.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"

class InfoBarService;

class InstantAppsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  ~InstantAppsInfoBarDelegate() override;

  static void Create(InfoBarService* infobar_service,
                     jobject jdata);

  base::android::ScopedJavaGlobalRef<jobject> data() { return data_; }

 private:
  explicit InstantAppsInfoBarDelegate(jobject jdata);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  bool Accept() override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;

  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
  base::android::ScopedJavaGlobalRef<jobject> data_;

  DISALLOW_COPY_AND_ASSIGN(InstantAppsInfoBarDelegate);
};

// Register native methods.
bool RegisterInstantAppsInfoBarDelegate(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_INFOBAR_DELEGATE_H_
