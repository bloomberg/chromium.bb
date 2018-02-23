// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_ambient_badge_infobar_delegate.h"

#include <memory>

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/installable_ambient_badge_infobar.h"

InstallableAmbientBadgeInfoBarDelegate::
    ~InstallableAmbientBadgeInfoBarDelegate() {}

// static
void InstallableAmbientBadgeInfoBarDelegate::Create(
    content::WebContents* web_contents,
    base::WeakPtr<Client> weak_client,
    const SkBitmap& primary_icon,
    const GURL& start_url,
    bool is_installed) {
  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(std::make_unique<InstallableAmbientBadgeInfoBar>(
          std::unique_ptr<InstallableAmbientBadgeInfoBarDelegate>(
              new InstallableAmbientBadgeInfoBarDelegate(
                  weak_client, primary_icon, start_url, is_installed))));
}

void InstallableAmbientBadgeInfoBarDelegate::AddToHomescreen() {
  if (!weak_client_.get())
    return;

  weak_client_->AddToHomescreenFromBadge();
}

const SkBitmap& InstallableAmbientBadgeInfoBarDelegate::GetPrimaryIcon() const {
  return primary_icon_;
}

InstallableAmbientBadgeInfoBarDelegate::InstallableAmbientBadgeInfoBarDelegate(
    base::WeakPtr<Client> weak_client,
    const SkBitmap& primary_icon,
    const GURL& start_url,
    bool is_installed)
    : infobars::InfoBarDelegate(),
      weak_client_(weak_client),
      primary_icon_(primary_icon),
      start_url_(start_url),
      is_installed_(is_installed) {}

infobars::InfoBarDelegate::InfoBarIdentifier
InstallableAmbientBadgeInfoBarDelegate::GetIdentifier() const {
  return INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE;
}
