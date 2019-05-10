// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_consistency_mode_manager_test_util.h"

#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/test/base/testing_profile.h"

std::unique_ptr<TestingProfile> BuildPreDiceProfile() {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(
      AccountConsistencyModeManagerFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<AccountConsistencyModeManager>(
            Profile::FromBrowserContext(context),
            /*auto_migrate_to_dice=*/false);
      }));
  return builder.Build();
}
