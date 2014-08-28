// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_warning_badge_service.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestExtensionWarningSet : public WarningService {
 public:
  explicit TestExtensionWarningSet(Profile* profile) : WarningService(profile) {
  }
  virtual ~TestExtensionWarningSet() {}

  void AddWarning(const Warning& warning) {
    WarningSet warnings;
    warnings.insert(warning);
    AddWarnings(warnings);
  }
};

class TestExtensionWarningBadgeService : public ExtensionWarningBadgeService {
 public:
  TestExtensionWarningBadgeService(Profile* profile,
                                   WarningService* warning_service)
      : ExtensionWarningBadgeService(profile),
        warning_service_(warning_service) {}
  virtual ~TestExtensionWarningBadgeService() {}

  virtual const std::set<Warning>&
  GetCurrentWarnings() const OVERRIDE {
    return warning_service_->warnings();
  }

 private:
  WarningService* warning_service_;
};

bool HasBadge(Profile* profile) {
  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(profile);
  return service->GetGlobalErrorByMenuItemCommandID(IDC_EXTENSION_ERRORS) !=
      NULL;
}

const char* ext1_id = "extension1";
const char* ext2_id = "extension2";

}  // namespace

// Check that no badge appears if it has been suppressed for a specific
// warning.
TEST(ExtensionWarningBadgeServiceTest, SuppressBadgeForCurrentWarnings) {
  TestingProfile profile;
  TestExtensionWarningSet warnings(&profile);
  TestExtensionWarningBadgeService badge_service(&profile, &warnings);
  warnings.AddObserver(&badge_service);

  // Insert first warning.
  warnings.AddWarning(Warning::CreateNetworkDelayWarning(ext1_id));
  EXPECT_TRUE(HasBadge(&profile));

  // Suppress first warning.
  badge_service.SuppressCurrentWarnings();
  EXPECT_FALSE(HasBadge(&profile));

  // Simulate deinstallation of extension.
  std::set<Warning::WarningType> to_clear =
      warnings.GetWarningTypesAffectingExtension(ext1_id);
  warnings.ClearWarnings(to_clear);
  EXPECT_FALSE(HasBadge(&profile));

  // Set first warning again and verify that not badge is shown this time.
  warnings.AddWarning(Warning::CreateNetworkDelayWarning(ext1_id));
  EXPECT_FALSE(HasBadge(&profile));

  // Set second warning and verify that it shows a badge.
  warnings.AddWarning(Warning::CreateNetworkConflictWarning(ext2_id));
  EXPECT_TRUE(HasBadge(&profile));

  warnings.RemoveObserver(&badge_service);
}

}   // namespace extensions
