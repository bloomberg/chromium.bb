// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"

namespace web_app {

// static
WebAppProvider* WebAppProvider::Get(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

WebAppProvider::WebAppProvider(PrefService* pref_service)
    : web_app_policy_manager_(
          std::make_unique<WebAppPolicyManager>(pref_service)) {
  // TODO(nigeltao): install default web apps as per http://crbug.com/855281
}

WebAppProvider::~WebAppProvider() = default;

}  // namespace web_app
