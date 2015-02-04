// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/android/infobars/app_banner_infobar.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace banners {

infobars::InfoBar* AppBannerInfoBarDelegate::CreateForWebApp(
    infobars::InfoBarManager* infobar_manager,
    const AppDelegate* app_delegate,
    const base::string16& app_name,
    const GURL& url) {
  scoped_ptr<AppBannerInfoBarDelegate> delegate(new AppBannerInfoBarDelegate(
      app_delegate,
      app_name,
      url));

  return infobar_manager->AddInfoBar(
      make_scoped_ptr(new AppBannerInfoBar(delegate.Pass(), url)));
}

AppBannerInfoBarDelegate::AppBannerInfoBarDelegate(
    const AppDelegate* helper,
    const base::string16& app_title,
    const GURL& url)
    : delegate_(helper),
      app_title_(app_title),
      url_(url),
      button_text_(l10n_util::GetStringUTF16(
          IDS_APP_INSTALL_ALERTS_ADD_TO_HOMESCREEN)) {
}

AppBannerInfoBarDelegate::~AppBannerInfoBarDelegate() {
}

base::string16 AppBannerInfoBarDelegate::GetMessageText() const {
  return app_title_;
}

int AppBannerInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 AppBannerInfoBarDelegate::GetButtonLabel(InfoBarButton button)
    const {
  return button_text_;
}

gfx::Image AppBannerInfoBarDelegate::GetIcon() const {
  DCHECK(delegate_);
  return delegate_->GetIcon();
}

void AppBannerInfoBarDelegate::InfoBarDismissed() {
  DCHECK(delegate_);
  delegate_->Block();
}

bool AppBannerInfoBarDelegate::Accept() {
  DCHECK(delegate_);
  delegate_->Install();
  return true;
}

}  // namespace banners
