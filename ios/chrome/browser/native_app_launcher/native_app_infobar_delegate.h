// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_INFOBAR_DELEGATE_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_types.h"
#include "url/gurl.h"

@class NativeAppNavigationController;
@protocol NativeAppNavigationControllerProtocol;
@class NSString;
@class UIImage;

namespace infobars {
class InfoBarManager;
}  // namespace infobars

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace native_app_infobar {
extern const CGSize kSmallIconSize;
}  // namespace native_app_infobar

// The delegate contains the information to create a
// NativeAppInstallerInfoBarView, a NativeAppLauncherInfoBarView or a
// NativeAppOpenPolicyInfoBarView.
class NativeAppInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  NativeAppInfoBarDelegate(id<NativeAppNavigationControllerProtocol> controller,
                           const GURL& page_url,
                           NativeAppControllerType type);
  ~NativeAppInfoBarDelegate() override;

  // Creates and adds a native app info bar to |manager|.
  static bool Create(infobars::InfoBarManager* manager,
                     id<NativeAppNavigationControllerProtocol> controller,
                     const GURL& page_url,
                     NativeAppControllerType type);

  NativeAppInfoBarDelegate* AsNativeAppInfoBarDelegate() override;
  base::string16 GetInstallText() const;
  base::string16 GetLaunchText() const;
  base::string16 GetOpenPolicyText() const;
  base::string16 GetOpenOnceText() const;
  base::string16 GetOpenAlwaysText() const;
  const GURL& GetIconURL() const;
  NSString* GetAppId() const;
  infobars::InfoBarDelegate::Type GetInfoBarType() const override;
  InfoBarIdentifier GetIdentifier() const override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;
  net::URLRequestContextGetter* GetRequestContextGetter();
  void FetchSmallAppIcon(void (^block)(UIImage*));
  // This function is made virtual for tests.
  virtual void UserPerformedAction(NativeAppActionType userAction);
  NativeAppControllerType GetControllerType() const;

 private:
  bool ShouldExpire(const NavigationDetails& details) const override;
  id<NativeAppNavigationControllerProtocol> controller_;
  net::URLRequestContextGetter* requestContextGetter_;
  GURL page_url_;
  NativeAppControllerType type_;
  DISALLOW_COPY_AND_ASSIGN(NativeAppInfoBarDelegate);
};

#endif  // IOS_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_INFOBAR_DELEGATE_H_
