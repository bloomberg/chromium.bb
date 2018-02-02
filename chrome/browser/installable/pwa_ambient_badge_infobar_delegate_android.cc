// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/pwa_ambient_badge_infobar_delegate_android.h"

#include <utility>

#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/android/infobars/pwa_ambient_badge_infobar.h"

PwaAmbientBadgeInfoBarDelegateAndroid::
    ~PwaAmbientBadgeInfoBarDelegateAndroid() {}

// static
void PwaAmbientBadgeInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents,
    std::unique_ptr<ShortcutInfo> info,
    const SkBitmap& primary_icon,
    const SkBitmap& badge_icon,
    bool is_webapk) {
  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(std::make_unique<PwaAmbientBadgeInfoBar>(
          std::unique_ptr<PwaAmbientBadgeInfoBarDelegateAndroid>(
              new PwaAmbientBadgeInfoBarDelegateAndroid(
                  std::move(info), primary_icon, badge_icon, is_webapk))));
}

void PwaAmbientBadgeInfoBarDelegateAndroid::AddToHomescreen() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());

  if (!web_contents)
    return;

  if (is_webapk_) {
    WebApkInstallService::Get(web_contents->GetBrowserContext())
        ->InstallAsync(web_contents, *shortcut_info_, primary_icon_,
                       badge_icon_,
                       InstallableMetrics::GetInstallSource(
                           web_contents, InstallTrigger::AMBIENT_BADGE));
  } else {
    ShortcutHelper::AddToLauncherWithSkBitmap(web_contents, *shortcut_info_,
                                              primary_icon_);
  }
}

const SkBitmap& PwaAmbientBadgeInfoBarDelegateAndroid::GetPrimaryIcon() const {
  return primary_icon_;
}

PwaAmbientBadgeInfoBarDelegateAndroid::PwaAmbientBadgeInfoBarDelegateAndroid(
    std::unique_ptr<ShortcutInfo> info,
    const SkBitmap& primary_icon,
    const SkBitmap& badge_icon,
    bool is_webapk)
    : infobars::InfoBarDelegate(),
      shortcut_info_(std::move(info)),
      primary_icon_(primary_icon),
      badge_icon_(badge_icon),
      is_webapk_(is_webapk) {}

infobars::InfoBarDelegate::InfoBarIdentifier
PwaAmbientBadgeInfoBarDelegateAndroid::GetIdentifier() const {
  return PWA_AMBIENT_BADGE_INFOBAR_DELEGATE_ANDROID;
}
