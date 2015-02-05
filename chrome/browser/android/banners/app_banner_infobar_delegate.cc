// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"

#include "base/android/jni_android.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/android/infobars/app_banner_infobar.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"

namespace banners {

// static
AppBannerInfoBar* AppBannerInfoBarDelegate::CreateForWebApp(
    infobars::InfoBarManager* infobar_manager,
    AppDelegate* app_delegate,
    const GURL& url) {
  scoped_ptr<AppBannerInfoBarDelegate> delegate(new AppBannerInfoBarDelegate(
      app_delegate));
  AppBannerInfoBar* infobar = new AppBannerInfoBar(delegate.Pass(), url);
  return infobar_manager->AddInfoBar(make_scoped_ptr(infobar))
      ? infobar : nullptr;
}

AppBannerInfoBarDelegate::AppBannerInfoBarDelegate(AppDelegate* app_delegate)
    : delegate_(app_delegate) {
}

AppBannerInfoBarDelegate::~AppBannerInfoBarDelegate() {
  DCHECK(delegate_);
  delegate_->OnInfoBarDestroyed();
}

base::string16 AppBannerInfoBarDelegate::GetMessageText() const {
  DCHECK(delegate_);
  return delegate_->GetTitle();
}

int AppBannerInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
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
  return delegate_->OnButtonClicked();
}

}  // namespace banners
