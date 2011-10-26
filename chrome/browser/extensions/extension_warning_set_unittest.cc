// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_warning_set.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ExtensionglobalError;

namespace {

class MockExtensionWarningSet : public ExtensionWarningSet {
 public:
  MockExtensionWarningSet() : ExtensionWarningSet(NULL) {
    ON_CALL(*this, ActivateBadge()).WillByDefault(
        testing::Invoke(this, &MockExtensionWarningSet::ActivateBadgeImpl));
    ON_CALL(*this, DeactivateBadge()).WillByDefault(
        testing::Invoke(this, &MockExtensionWarningSet::DeactivateBadgeImpl));
  }
  virtual ~MockExtensionWarningSet() {}

  MOCK_METHOD0(NotifyWarningsChanged, void());
  MOCK_METHOD0(ActivateBadge, void());
  MOCK_METHOD0(DeactivateBadge, void());

  void ActivateBadgeImpl() {
    // Just fill the value so that we see that something would be there
    // in a non-mocked execution.
    extension_global_error_badge_ =
        reinterpret_cast<ExtensionGlobalErrorBadge*>(1);
  }
  void DeactivateBadgeImpl() {
    extension_global_error_badge_ = NULL;
  }
};

const char* ext1_id = "extension1";
const char* ext2_id = "extension2";
const ExtensionWarningSet::WarningType warning_1 =
    ExtensionWarningSet::kNetworkDelay;
const ExtensionWarningSet::WarningType warning_2 =
    ExtensionWarningSet::kNetworkConflict;

}  // namespace

// Check that inserting a warning triggers notifications, whereas inserting
// the same warning again is silent.
TEST(ExtensionWarningSet, SetWarning) {
  MockExtensionWarningSet warnings;

  // Insert warning for the first time.
  EXPECT_CALL(warnings, NotifyWarningsChanged());
  EXPECT_CALL(warnings, ActivateBadge());
  warnings.SetWarning(warning_1, ext1_id);
  testing::Mock::VerifyAndClearExpectations(&warnings);

  // Second insertion of same warning does not trigger anything.
  warnings.SetWarning(warning_1, ext1_id);
  testing::Mock::VerifyAndClearExpectations(&warnings);
}

// Check that ClearWarnings deletes exactly the specified warnings and
// triggers notifications where appropriate.
TEST(ExtensionWarningSet, ClearWarnings) {
  MockExtensionWarningSet warnings;

  // Insert two unique warnings.
  EXPECT_CALL(warnings, NotifyWarningsChanged()).Times(2);
  EXPECT_CALL(warnings, ActivateBadge()).Times(1);
  warnings.SetWarning(warning_1, ext1_id);
  warnings.SetWarning(warning_2, ext2_id);
  testing::Mock::VerifyAndClearExpectations(&warnings);

  // Remove one warning and check that the badge remains.
  EXPECT_CALL(warnings, NotifyWarningsChanged());
  std::set<ExtensionWarningSet::WarningType> to_clear;
  to_clear.insert(warning_2);
  warnings.ClearWarnings(to_clear);
  testing::Mock::VerifyAndClearExpectations(&warnings);

  // Check that the correct warnings appear in |warnings|.
  std::set<ExtensionWarningSet::WarningType> existing_warnings;
  warnings.GetWarningsAffectingExtension(ext1_id, &existing_warnings);
  EXPECT_EQ(1u, existing_warnings.size());
  warnings.GetWarningsAffectingExtension(ext2_id, &existing_warnings);
  EXPECT_EQ(0u, existing_warnings.size());

  // Remove the other one warning and check that badge disappears.
  EXPECT_CALL(warnings, NotifyWarningsChanged());
  EXPECT_CALL(warnings, DeactivateBadge());
  to_clear.insert(warning_1);
  warnings.ClearWarnings(to_clear);
  testing::Mock::VerifyAndClearExpectations(&warnings);

  // Check that not warnings remain.
  warnings.GetWarningsAffectingExtension(ext1_id, &existing_warnings);
  EXPECT_EQ(0u, existing_warnings.size());
  warnings.GetWarningsAffectingExtension(ext2_id, &existing_warnings);
  EXPECT_EQ(0u, existing_warnings.size());
}

// Check that no badge appears if it has been suppressed for a specific
// warning.
TEST(ExtensionWarningSet, SuppressBadgeForCurrentWarnings) {
  MockExtensionWarningSet warnings;

  // Insert first warning.
  EXPECT_CALL(warnings, NotifyWarningsChanged());
  EXPECT_CALL(warnings, ActivateBadge());
  warnings.SetWarning(warning_1, ext1_id);
  testing::Mock::VerifyAndClearExpectations(&warnings);

  // Suppress first warning.
  EXPECT_CALL(warnings, DeactivateBadge());
  warnings.SuppressBadgeForCurrentWarnings();
  testing::Mock::VerifyAndClearExpectations(&warnings);

  // Simulate deinstallation of extension.
  std::set<ExtensionWarningSet::WarningType> to_clear;
  warnings.GetWarningsAffectingExtension(ext1_id, &to_clear);
  EXPECT_CALL(warnings, NotifyWarningsChanged());
  warnings.ClearWarnings(to_clear);
  testing::Mock::VerifyAndClearExpectations(&warnings);

  // Set first warning again and verify that not badge is shown this time.
  EXPECT_CALL(warnings, NotifyWarningsChanged());
  warnings.SetWarning(warning_1, ext1_id);
  testing::Mock::VerifyAndClearExpectations(&warnings);

  // Set second warning and verify that it shows a badge.
  EXPECT_CALL(warnings, NotifyWarningsChanged());
  EXPECT_CALL(warnings, ActivateBadge());
  warnings.SetWarning(warning_2, ext2_id);
  testing::Mock::VerifyAndClearExpectations(&warnings);
}
