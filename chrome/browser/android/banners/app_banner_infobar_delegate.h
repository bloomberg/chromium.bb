// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_INFOBAR_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/common/manifest.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace infobars {
class InfoBarManager;
}  // namespace infobars

class AppBannerInfoBar;

namespace banners {

// Manages installation of an app being promoted by a webpage.
class AppBannerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Delegate for promoting a web app.
  AppBannerInfoBarDelegate(
      const base::string16& app_title,
      SkBitmap* app_icon,
      const content::Manifest& web_app_data);

  // Delegate for promoting an Android app.
  AppBannerInfoBarDelegate(
      const base::string16& app_title,
      SkBitmap* app_icon,
      const base::android::ScopedJavaGlobalRef<jobject>& native_app_data,
      const std::string& native_app_package);

  ~AppBannerInfoBarDelegate() override;

  // Called when the AppBannerInfoBar's button needs to be updated.
  void UpdateInstallState(JNIEnv* env, jobject obj);

  // Called when the installation Intent has been handled and focus has been
  // returned to Chrome.
  void OnInstallIntentReturned(JNIEnv* env,
                               jobject obj,
                               jboolean jis_installing);

  // Called when the InstallerDelegate task has finished.
  void OnInstallFinished(JNIEnv* env,
                         jobject obj,
                         jboolean success);

 private:
  void CreateJavaDelegate();

  // ConfirmInfoBarDelegate:
  gfx::Image GetIcon() const override;
  void InfoBarDismissed() override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  bool Accept() override;
  bool LinkClicked(WindowOpenDisposition disposition) override;

  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;

  base::string16 app_title_;
  scoped_ptr<SkBitmap> app_icon_;

  content::Manifest web_app_data_;

  base::android::ScopedJavaGlobalRef<jobject> native_app_data_;
  std::string native_app_package_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerInfoBarDelegate);
};  // AppBannerInfoBarDelegate

// Register native methods.
bool RegisterAppBannerInfoBarDelegate(JNIEnv* env);

}  // namespace banners

#endif  // CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_INFOBAR_DELEGATE_H_
