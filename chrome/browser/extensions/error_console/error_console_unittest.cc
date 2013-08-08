// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/error_console/extension_error.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/id_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::string16;
using base::UTF8ToUTF16;

namespace extensions {

namespace {

scoped_ptr<ExtensionError> CreateNewManifestError(bool from_incognito) {
  return scoped_ptr<ExtensionError>(
      new ManifestParsingError(from_incognito,
                               UTF8ToUTF16("source"),
                               UTF8ToUTF16("message"),
                               0u /* line number */ ));
}

}  // namespace

class ErrorConsoleUnitTest : public testing::Test {
 public:
  ErrorConsoleUnitTest() :
      profile_(new TestingProfile),
      error_console_(ErrorConsole::Get(profile_.get())) {
  }
  virtual ~ErrorConsoleUnitTest() { }

 protected:
  scoped_ptr<TestingProfile> profile_;
  ErrorConsole* error_console_;
};

// Test adding errors, and removing them by reference, by incognito status,
// and in bulk.
TEST_F(ErrorConsoleUnitTest, AddAndRemoveErrors) {
  ASSERT_EQ(0u, error_console_->errors().size());

  const size_t kNumTotalErrors = 6;
  const size_t kNumNonIncognitoErrors = 3;
  // Populate with both incognito and non-incognito errors (evenly distributed).
  for (size_t i = 0; i < kNumTotalErrors; ++i)
    error_console_->ReportError(CreateNewManifestError(i % 2 == 0));

  ASSERT_EQ(kNumTotalErrors, error_console_->errors().size());

  // Remove the incognito errors; three errors should remain, and all should
  // be from non-incognito contexts.
  error_console_->RemoveIncognitoErrors();
  ASSERT_EQ(kNumNonIncognitoErrors, error_console_->errors().size());
  for (size_t i = 0; i < error_console_->errors().size(); ++i)
    ASSERT_FALSE(error_console_->errors()[i]->from_incognito());

  // Remove an error by address.
  error_console_->RemoveError(error_console_->errors()[1]);
  ASSERT_EQ(kNumNonIncognitoErrors - 1, error_console_->errors().size());

  // Remove all remaining errors.
  error_console_->RemoveAllErrors();
  ASSERT_EQ(0u, error_console_->errors().size());
}

}  // namespace extensions
