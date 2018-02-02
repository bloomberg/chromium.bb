// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/pwa_ambient_badge_manager_android.h"

#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/chrome_webapk_host.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/pwa_ambient_badge_infobar_delegate_android.h"

namespace {

InstallableParams GetInstallableParams(bool can_install_webapk) {
  InstallableParams params;
  params.check_eligibility = true;
  params.valid_primary_icon = true;
  params.valid_badge_icon = can_install_webapk;
  params.valid_manifest = true;
  params.has_worker = true;
  params.wait_for_worker = true;

  return params;
}

}  // anonymous namespace

PwaAmbientBadgeManagerAndroid::PwaAmbientBadgeManagerAndroid(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      installable_manager_(InstallableManager::FromWebContents(contents)),
      can_install_webapk_(ChromeWebApkHost::CanInstallWebApk()),
      weak_factory_(this) {
  DCHECK(installable_manager_);
}

PwaAmbientBadgeManagerAndroid::~PwaAmbientBadgeManagerAndroid() {}

void PwaAmbientBadgeManagerAndroid::MaybeShowBadge() {
  installable_manager_->GetData(
      GetInstallableParams(can_install_webapk_),
      base::Bind(&PwaAmbientBadgeManagerAndroid::OnDidFinishInstallableCheck,
                 weak_factory_.GetWeakPtr()));
}

void PwaAmbientBadgeManagerAndroid::OnDidFinishInstallableCheck(
    const InstallableData& data) {
  content::WebContents* contents = web_contents();
  if (data.error_code != NO_ERROR_DETECTED || !contents)
    return;

  GURL badge_icon_url;
  SkBitmap badge_icon;

  if (data.badge_icon && !data.badge_icon->drawsNothing()) {
    badge_icon_url = data.badge_icon_url;
    badge_icon = *data.badge_icon;
  }

  PwaAmbientBadgeInfoBarDelegateAndroid::Create(
      contents,
      ShortcutHelper::CreateShortcutInfo(data.manifest_url, *data.manifest,
                                         data.primary_icon_url, badge_icon_url),
      *data.primary_icon, badge_icon, can_install_webapk_);
}
