// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_INFOBAR_DELEGATE_H_

#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace infobars {
class InfoBarManager;
}  // namespace infobars

namespace banners {

// Displays information about an app being promoted by a webpage.
class AppBannerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Handles calls dealing with blocking, promoting, or grabbing info about an
  // app.
  class AppDelegate {
   public:
    // User has elected to block the banner from being displayed.
    virtual void Block() const = 0;

    // User has requested that the app be installed.
    virtual void Install() const = 0;

    // Icon to display for the app.
    virtual gfx::Image GetIcon() const = 0;
  };

  // Creates a banner for the current page.
  // May return nullptr if the the infobar couldn't be created.
  static infobars::InfoBar* CreateForWebApp(
      infobars::InfoBarManager* infobar_manager,
      const AppDelegate* delegate,
      const base::string16& app_title,
      const GURL& url);

  ~AppBannerInfoBarDelegate() override;

  // Changes the label of the button.
  void SetButtonLabel(const std::string& button_text);

  // InfoBarDelegate overrides.
  gfx::Image GetIcon() const override;
  void InfoBarDismissed() override;

  // ConfirmInfoBarDelegate overrides.
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

 private:
  // Constructor for a banner for web apps.
  AppBannerInfoBarDelegate(const AppDelegate* helper,
                           const base::string16& app_title,
                           const GURL& url);

  const AppDelegate* delegate_;
  base::string16 app_title_;
  GURL url_;
  base::string16 button_text_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerInfoBarDelegate);
};  // AppBannerInfoBarDelegate

}  // namespace banners

bool RegisterAppBannerInfoBarDelegate(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_INFOBAR_DELEGATE_H_
