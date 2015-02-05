// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_INFOBAR_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace infobars {
class InfoBarManager;
}  // namespace infobars

class AppBannerInfoBar;

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

    // User has clicked the button.
    // Returns true if the infobar should be dismissed.
    virtual bool OnButtonClicked() const = 0;

    // Called when the infobar has been destroyed.
    virtual void OnInfoBarDestroyed() = 0;

    // Returns the title of the app.
    virtual base::string16 GetTitle() const = 0;

    // Returns the icon to display for the app.
    virtual gfx::Image GetIcon() const = 0;
  };

  // Creates a banner for the current page that promotes a web app.
  // May return nullptr if the the infobar couldn't be created.
  static AppBannerInfoBar* CreateForWebApp(
      infobars::InfoBarManager* infobar_manager,
      AppDelegate* delegate,
      const GURL& url);

  ~AppBannerInfoBarDelegate() override;

  // InfoBarDelegate overrides.
  gfx::Image GetIcon() const override;
  void InfoBarDismissed() override;

  // ConfirmInfoBarDelegate overrides.
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  bool Accept() override;

 private:
  explicit AppBannerInfoBarDelegate(AppDelegate* delegate);

  AppDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerInfoBarDelegate);
};  // AppBannerInfoBarDelegate

}  // namespace banners

bool RegisterAppBannerInfoBarDelegate(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_INFOBAR_DELEGATE_H_
