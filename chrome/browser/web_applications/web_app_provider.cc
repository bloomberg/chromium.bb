// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include <utility>

#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"

namespace web_app {

// static
WebAppProvider* WebAppProvider::Get(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

WebAppProvider::WebAppProvider(PrefService* pref_service)
    : pending_app_manager_(
          std::make_unique<extensions::PendingBookmarkAppManager>()),
      web_app_policy_manager_(
          std::make_unique<WebAppPolicyManager>(pref_service,
                                                pending_app_manager_.get())) {
  // TODO(nigeltao): install default web apps as per http://crbug.com/855281
}

WebAppProvider::~WebAppProvider() = default;

// static
void WebAppProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  WebAppPolicyManager::RegisterProfilePrefs(registry);
}

}  // namespace web_app
