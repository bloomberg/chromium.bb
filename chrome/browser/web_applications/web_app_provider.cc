// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/bookmark_apps/external_web_apps.h"
#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"

namespace web_app {

// static
WebAppProvider* WebAppProvider::Get(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

WebAppProvider::WebAppProvider(Profile* profile)
    : pending_app_manager_(
          std::make_unique<extensions::PendingBookmarkAppManager>(profile)),
      web_app_policy_manager_(
          std::make_unique<WebAppPolicyManager>(profile->GetPrefs(),
                                                pending_app_manager_.get())) {
  web_app::ScanForExternalWebApps(
      base::BindOnce(&WebAppProvider::OnScanForExternalWebApps,
                     weak_ptr_factory_.GetWeakPtr()));
}

WebAppProvider::~WebAppProvider() = default;

// static
void WebAppProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  WebAppPolicyManager::RegisterProfilePrefs(registry);
}

void WebAppProvider::OnScanForExternalWebApps(
    std::vector<web_app::PendingAppManager::AppInfo> app_infos) {
  pending_app_manager_->InstallApps(std::move(app_infos), base::DoNothing());
}

}  // namespace web_app
