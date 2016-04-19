// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_INFOBAR_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_INFOBAR_DELEGATE_DESKTOP_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/common/manifest.h"

namespace content {
class WebContents;
}  // namespace content

namespace extensions {
class BookmarkAppHelper;
class Extension;
}  // namespace extensions

namespace infobars {
class InfoBar;
}  // namespace infobars

namespace banners {

class AppBannerDataFetcherDesktop;

class AppBannerInfoBarDelegateDesktop : public ConfirmInfoBarDelegate {

 public:
  ~AppBannerInfoBarDelegateDesktop() override;

  static infobars::InfoBar* Create(
      scoped_refptr<AppBannerDataFetcherDesktop> fetcher,
      content::WebContents* web_contents,
      const content::Manifest& web_manifest,
      extensions::BookmarkAppHelper* bookmark_app_helper,
      int event_request_id);

  // ConfirmInfoBarDelegate overrides.
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;

  bool Accept() override;

  // InfoBarDelegate override.
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  void InfoBarDismissed() override;

 protected:
  AppBannerInfoBarDelegateDesktop(
      scoped_refptr<AppBannerDataFetcherDesktop> fetcher,
      const content::Manifest& web_manifest,
      extensions::BookmarkAppHelper* bookmark_app_helper,
      int event_request_id);

 private:
  scoped_refptr<AppBannerDataFetcherDesktop> fetcher_;
  content::Manifest web_manifest_;
  extensions::BookmarkAppHelper* bookmark_app_helper_;
  int event_request_id_;
  bool has_user_interaction_;

  Type GetInfoBarType() const override;
  int GetIconId() const override;
  gfx::VectorIconId GetVectorIconId() const override;

  DISALLOW_COPY_AND_ASSIGN(AppBannerInfoBarDelegateDesktop);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_INFOBAR_DELEGATE_DESKTOP_H_
