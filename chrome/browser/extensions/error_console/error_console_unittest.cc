// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_error_test_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/id_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using error_test_util::CreateNewManifestError;
using error_test_util::CreateNewRuntimeError;

class ErrorConsoleUnitTest : public testing::Test {
 public:
  ErrorConsoleUnitTest() : error_console_(NULL) { }
  virtual ~ErrorConsoleUnitTest() { }

  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();

    // Errors are only kept if we have the FeatureSwitch and have Developer Mode
    // enabled.
    FeatureSwitch::error_console()->SetOverrideValue(
        FeatureSwitch::OVERRIDE_ENABLED);
    profile_.reset(new TestingProfile);
    profile_->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
    error_console_ = ErrorConsole::Get(profile_.get());
  }

 protected:
  scoped_ptr<TestingProfile> profile_;
  ErrorConsole* error_console_;
};

// Test that errors are successfully reported. This is a simple test, since it
// is tested more thoroughly in extensions/browser/error_map_unittest.cc
TEST_F(ErrorConsoleUnitTest, ReportErrors) {
  const size_t kNumTotalErrors = 6;
  const std::string kId = id_util::GenerateId("id");
  ASSERT_EQ(0u, error_console_->GetErrorsForExtension(kId).size());

  for (size_t i = 0; i < kNumTotalErrors; ++i) {
    error_console_->ReportError(
        CreateNewManifestError(kId, base::UintToString(i)));
  }

  ASSERT_EQ(kNumTotalErrors, error_console_->GetErrorsForExtension(kId).size());
}

TEST_F(ErrorConsoleUnitTest, DontStoreErrorsWithoutEnablingType) {
  // Disable default runtime error reporting.
  error_console_->set_default_reporting_for_test(ExtensionError::RUNTIME_ERROR,
                                                false);

  const std::string kId = id_util::GenerateId("id");

  // Try to report a runtime error - it should be ignored.
  error_console_->ReportError(CreateNewRuntimeError(kId, "a"));
  ASSERT_EQ(0u, error_console_->GetErrorsForExtension(kId).size());

  // Check that manifest errors are reported successfully.
  error_console_->ReportError(CreateNewManifestError(kId, "b"));
  ASSERT_EQ(1u, error_console_->GetErrorsForExtension(kId).size());

  // We should still ignore runtime errors.
  error_console_->ReportError(CreateNewRuntimeError(kId, "c"));
  ASSERT_EQ(1u, error_console_->GetErrorsForExtension(kId).size());

  // Enable runtime errors specifically for this extension, and disable the use
  // of the default mask.
  error_console_->SetReportingForExtension(
      kId, ExtensionError::RUNTIME_ERROR, true);

  // We should now accept runtime and manifest errors.
  error_console_->ReportError(CreateNewManifestError(kId, "d"));
  ASSERT_EQ(2u, error_console_->GetErrorsForExtension(kId).size());
  error_console_->ReportError(CreateNewRuntimeError(kId, "e"));
  ASSERT_EQ(3u, error_console_->GetErrorsForExtension(kId).size());

  // All other extensions should still use the default mask, and ignore runtime
  // errors but report manifest errors.
  const std::string kId2 = id_util::GenerateId("id2");
  error_console_->ReportError(CreateNewRuntimeError(kId2, "f"));
  ASSERT_EQ(0u, error_console_->GetErrorsForExtension(kId2).size());
  error_console_->ReportError(CreateNewManifestError(kId2, "g"));
  ASSERT_EQ(1u, error_console_->GetErrorsForExtension(kId2).size());

  // By reverting to default reporting, we should again allow manifest errors,
  // but not runtime errors.
  error_console_->UseDefaultReportingForExtension(kId);
  error_console_->ReportError(CreateNewManifestError(kId, "h"));
  ASSERT_EQ(4u, error_console_->GetErrorsForExtension(kId).size());
  error_console_->ReportError(CreateNewRuntimeError(kId, "i"));
  ASSERT_EQ(4u, error_console_->GetErrorsForExtension(kId).size());
}

}  // namespace extensions
