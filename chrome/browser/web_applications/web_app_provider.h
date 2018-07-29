// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"

class Profile;

namespace web_app {

class WebAppPolicyManager;

// Connects Web App features, such as the installation of default and
// policy-managed web apps, with Profiles (as WebAppProvider is a
// Profile-linked KeyedService) and their associated PrefService.
class WebAppProvider : public KeyedService {
 public:
  static WebAppProvider* Get(Profile* profile);

  explicit WebAppProvider(PrefService* pref_service);

  ~WebAppProvider() override;

 private:
  void ScanForExternalWebApps();
  void ScanForExternalWebAppsCallback(
      std::vector<web_app::PendingAppManager::AppInfo>);

  std::unique_ptr<WebAppPolicyManager> web_app_policy_manager_;

  base::WeakPtrFactory<WebAppProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppProvider);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
