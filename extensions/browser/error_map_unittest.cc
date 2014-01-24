// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/error_map.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_error_test_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/id_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using error_test_util::CreateNewRuntimeError;

class ErrorMapUnitTest : public testing::Test {
 public:
  ErrorMapUnitTest() { }
  virtual ~ErrorMapUnitTest() { }

  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();
  }

 protected:
  ErrorMap errors_;
};

// Test adding errors, and removing them by reference, by incognito status,
// and in bulk.
TEST_F(ErrorMapUnitTest, AddAndRemoveErrors) {
  ASSERT_EQ(0u, errors_.size());

  const size_t kNumTotalErrors = 6;
  const size_t kNumNonIncognitoErrors = 3;
  const std::string kId = id_util::GenerateId("id");
  // Populate with both incognito and non-incognito errors (evenly distributed).
  for (size_t i = 0; i < kNumTotalErrors; ++i) {
    ASSERT_TRUE(errors_.AddError(
        CreateNewRuntimeError(kId, base::UintToString(i), i % 2 == 0)));
  }

  // There should only be one entry in the map, since errors are stored in lists
  // keyed by extension id.
  ASSERT_EQ(1u, errors_.size());

  ASSERT_EQ(kNumTotalErrors, errors_.GetErrorsForExtension(kId).size());

  // Remove the incognito errors; three errors should remain, and all should
  // be from non-incognito contexts.
  errors_.RemoveIncognitoErrors();
  const ErrorList& list = errors_.GetErrorsForExtension(kId);
  ASSERT_EQ(kNumNonIncognitoErrors, list.size());
  for (size_t i = 0; i < list.size(); ++i)
    ASSERT_FALSE(list[i]->from_incognito());

  // Add another error for a different extension id.
  const std::string kSecondId = id_util::GenerateId("id2");
  ASSERT_TRUE(errors_.AddError(CreateNewRuntimeError(kSecondId, "foo")));

  // There should be two entries now, one for each id, and there should be one
  // error for the second extension.
  ASSERT_EQ(2u, errors_.size());
  ASSERT_EQ(1u, errors_.GetErrorsForExtension(kSecondId).size());

  // Remove all errors for the second id.
  errors_.Remove(kSecondId);
  ASSERT_EQ(1u, errors_.size());
  ASSERT_EQ(0u, errors_.GetErrorsForExtension(kSecondId).size());
  // First extension should be unaffected.
  ASSERT_EQ(kNumNonIncognitoErrors,
            errors_.GetErrorsForExtension(kId).size());

  // Remove remaining errors.
  errors_.RemoveAllErrors();
  ASSERT_EQ(0u, errors_.size());
  ASSERT_EQ(0u, errors_.GetErrorsForExtension(kId).size());
}

// Test that if we add enough errors, only the most recent
// kMaxErrorsPerExtension are kept.
TEST_F(ErrorMapUnitTest, ExcessiveErrorsGetCropped) {
  ASSERT_EQ(0u, errors_.size());

  // This constant matches one of the same name in error_console.cc.
  const size_t kMaxErrorsPerExtension = 100;
  const size_t kNumExtraErrors = 5;
  const std::string kId = id_util::GenerateId("id");

  // Add new errors, with each error's message set to its number.
  for (size_t i = 0; i < kMaxErrorsPerExtension + kNumExtraErrors; ++i) {
    ASSERT_TRUE(errors_.AddError(
        CreateNewRuntimeError(kId, base::UintToString(i))));
  }

  ASSERT_EQ(1u, errors_.size());

  const ErrorList& list = errors_.GetErrorsForExtension(kId);
  ASSERT_EQ(kMaxErrorsPerExtension, list.size());

  // We should have popped off errors in the order they arrived, so the
  // first stored error should be the 6th reported (zero-based)...
  ASSERT_EQ(base::UintToString16(kNumExtraErrors),
            list.front()->message());
  // ..and the last stored should be the 105th reported.
  ASSERT_EQ(base::UintToString16(kMaxErrorsPerExtension + kNumExtraErrors - 1),
            list.back()->message());
}

// Test to ensure that the error console will not add duplicate errors, but will
// keep the latest version of an error.
TEST_F(ErrorMapUnitTest, DuplicateErrorsAreReplaced) {
  ASSERT_EQ(0u, errors_.size());

  const std::string kId = id_util::GenerateId("id");
  const size_t kNumErrors = 3u;

  // Report three errors.
  for (size_t i = 0; i < kNumErrors; ++i) {
    ASSERT_TRUE(errors_.AddError(
        CreateNewRuntimeError(kId, base::UintToString(i))));
  }

  // Create an error identical to the second error reported, save its
  // location, and add it to the error map.
  scoped_ptr<ExtensionError> runtime_error2 =
      CreateNewRuntimeError(kId, base::UintToString(1u));
  const ExtensionError* weak_error = runtime_error2.get();
  ASSERT_TRUE(errors_.AddError(runtime_error2.Pass()));

  // We should only have three errors stored, since two of the four reported
  // were identical, and the older should have been replaced.
  ASSERT_EQ(1u, errors_.size());
  const ErrorList& list = errors_.GetErrorsForExtension(kId);
  ASSERT_EQ(kNumErrors, list.size());

  // The duplicate error should be the last reported (pointer comparison)...
  ASSERT_EQ(weak_error, list.back());
  // ... and should have two reported occurrences.
  ASSERT_EQ(2u, list.back()->occurrences());
}

}  // namespace extensions
