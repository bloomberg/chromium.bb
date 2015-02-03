// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/l10n/l10n_util.h"

namespace banners {

infobars::InfoBar* AppBannerInfoBarDelegate::CreateForWebApp(
    InfoBarService* infobar_service,
    const AppDelegate* helper,
    const base::string16& app_name,
    const GURL& url) {
  AppBannerInfoBarDelegate* const delegate = new AppBannerInfoBarDelegate(
      helper,
      app_name,
      url);

  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(delegate)));
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
