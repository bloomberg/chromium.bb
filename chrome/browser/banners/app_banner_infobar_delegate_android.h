// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_INFOBAR_DELEGATE_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class WebContents;
}

namespace infobars {
class InfoBarManager;
}

enum class WebApkInstallResult;
struct ShortcutInfo;

namespace banners {

class AppBannerManager;

// Manages installation of an app being promoted by a page.
class AppBannerInfoBarDelegateAndroid : public ConfirmInfoBarDelegate {
 public:
  // Creates an infobar and delegate for promoting the installation of a web
  // app, and adds the infobar to the InfoBarManager for |web_contents|.
  static bool Create(content::WebContents* web_contents,
                     base::WeakPtr<AppBannerManager> weak_manager,
                     std::unique_ptr<ShortcutInfo> info,
                     const SkBitmap& primary_icon,
                     const SkBitmap& badge_icon,
                     bool is_webapk);

  // Creates an infobar and delegate for promoting the installation of an
  // Android app, and adds the infobar to the InfoBarManager for |web_contents|.
  static bool Create(
      content::WebContents* web_contents,
      const base::string16& app_title,
      const base::android::ScopedJavaGlobalRef<jobject>& native_app_data,
      const SkBitmap& icon,
      const std::string& native_app_package_name,
      const std::string& referrer);

  ~AppBannerInfoBarDelegateAndroid() override;

  // Called when the AppBannerInfoBarAndroid's button needs to be updated.
  void UpdateInstallState(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  // Called when the native app installation Intent has been handled and focus
  // has been returned to Chrome.
  void OnInstallIntentReturned(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               jboolean jis_installing);

  // Called when the native app InstallerDelegate task has finished.
  void OnInstallFinished(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jboolean success);

  const SkBitmap& GetPrimaryIcon() const;

  // ConfirmInfoBarDelegate:
  bool Accept() override;
  base::string16 GetMessageText() const override;

 private:
  // Delegate for promoting a web app.
  AppBannerInfoBarDelegateAndroid(base::WeakPtr<AppBannerManager> weak_manager,
                                  std::unique_ptr<ShortcutInfo> info,
                                  const SkBitmap& primary_icon,
                                  const SkBitmap& badge_icon,
                                  bool is_webapk);

  // Delegate for promoting an Android app.
  AppBannerInfoBarDelegateAndroid(
      const base::string16& app_title,
      const base::android::ScopedJavaGlobalRef<jobject>& native_app_data,
      const SkBitmap& icon,
      const std::string& native_app_package_name,
      const std::string& referrer);

  void CreateJavaDelegate();
  bool AcceptNativeApp(content::WebContents* web_contents);
  bool AcceptWebApp(content::WebContents* web_contents);

  // Called when the OK button on a WebAPK infobar is pressed. If the WebAPK is
  // already installed, opens it; otherwise, installs it. Returns whether the
  // infobar should be closed as a result of the button press.
  bool AcceptWebApk(content::WebContents* web_contents);

  // Called when the user accepts the banner to install the app. (Not called
  // when the "Open" button is pressed on the banner that is shown after
  // installation for WebAPK banners.)
  void SendBannerAccepted();

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  void InfoBarDismissed() override;
  int GetButtons() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;

  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;

  // Used to fetch the splash screen icon for webapps.
  base::WeakPtr<AppBannerManager> weak_manager_;

  base::string16 app_title_;
  std::unique_ptr<ShortcutInfo> shortcut_info_;

  base::android::ScopedJavaGlobalRef<jobject> native_app_data_;

  const SkBitmap primary_icon_;
  const SkBitmap badge_icon_;

  std::string package_name_;
  std::string referrer_;
  bool has_user_interaction_;

  bool is_webapk_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerInfoBarDelegateAndroid);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_INFOBAR_DELEGATE_ANDROID_H_
