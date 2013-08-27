// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_error.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::string16;
using base::UTF8ToUTF16;

namespace extensions {

namespace {

const char kExecutionContextURLKey[] = "executionContextURL";
const char kStackTraceKey[] = "stackTrace";

string16 CreateErrorDetails(const std::string& extension_id) {
  base::DictionaryValue value;
  value.SetString(
      kExecutionContextURLKey,
      std::string(kExtensionScheme) +
          content::kStandardSchemeSeparator +
          extension_id);
  value.Set(kStackTraceKey, new ListValue);
  std::string json_utf8;
  base::JSONWriter::Write(&value, &json_utf8);
  return UTF8ToUTF16(json_utf8);
}

scoped_ptr<const ExtensionError> CreateNewRuntimeError(
    bool from_incognito,
    const std::string& extension_id,
    const string16& message) {
  return scoped_ptr<const ExtensionError>(new JavascriptRuntimeError(
      from_incognito,
      UTF8ToUTF16("source"),
      message,
      logging::LOG_INFO,
      CreateErrorDetails(extension_id)));
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
  const char kId[] = "id";
  // Populate with both incognito and non-incognito errors (evenly distributed).
  for (size_t i = 0; i < kNumTotalErrors; ++i) {
    error_console_->ReportError(
        CreateNewRuntimeError(i % 2 == 0, kId, string16()));
  }

  // There should only be one entry in the map, since errors are stored in lists
  // keyed by extension id.
  ASSERT_EQ(1u, error_console_->errors().size());

  ASSERT_EQ(kNumTotalErrors, error_console_->GetErrorsForExtension(kId).size());

  // Remove the incognito errors; three errors should remain, and all should
  // be from non-incognito contexts.
  error_console_->RemoveIncognitoErrors();
  const ErrorConsole::ErrorList& errors =
      error_console_->GetErrorsForExtension(kId);
  ASSERT_EQ(kNumNonIncognitoErrors, errors.size());
  for (size_t i = 0; i < errors.size(); ++i)
    ASSERT_FALSE(errors[i]->from_incognito());

  // Add another error for a different extension id.
  const char kSecondId[] = "id2";
  error_console_->ReportError(
      CreateNewRuntimeError(false, kSecondId, string16()));

  // There should be two entries now, one for each id, and there should be one
  // error for the second extension.
  ASSERT_EQ(2u, error_console_->errors().size());
  ASSERT_EQ(1u, error_console_->GetErrorsForExtension(kSecondId).size());

  // Remove all errors for the second id.
  error_console_->RemoveErrorsForExtension(kSecondId);
  ASSERT_EQ(1u, error_console_->errors().size());
  ASSERT_EQ(0u, error_console_->GetErrorsForExtension(kSecondId).size());
  // First extension should be unaffected.
  ASSERT_EQ(kNumNonIncognitoErrors,
            error_console_->GetErrorsForExtension(kId).size());

  // Remove remaining errors.
  error_console_->RemoveAllErrors();
  ASSERT_EQ(0u, error_console_->errors().size());
  ASSERT_EQ(0u, error_console_->GetErrorsForExtension(kId).size());
}

// Test that if we add enough errors, only the most recent
// kMaxErrorsPerExtension are kept.
TEST_F(ErrorConsoleUnitTest, ExcessiveErrorsGetCropped) {
  ASSERT_EQ(0u, error_console_->errors().size());

  // This constant matches one of the same name in error_console.cc.
  const size_t kMaxErrorsPerExtension = 100;
  const size_t kNumExtraErrors = 5;
  const char kId[] = "id";

  // Add new errors, with each error's message set to its number.
  for (size_t i = 0; i < kMaxErrorsPerExtension + kNumExtraErrors; ++i) {
    error_console_->ReportError(
        CreateNewRuntimeError(false, kId, base::UintToString16(i)));
  }

  ASSERT_EQ(1u, error_console_->errors().size());

  const ErrorConsole::ErrorList& errors =
      error_console_->GetErrorsForExtension(kId);
  ASSERT_EQ(kMaxErrorsPerExtension, errors.size());

  // We should have popped off errors in the order they arrived, so the
  // first stored error should be the 6th reported (zero-based)...
  ASSERT_EQ(errors.front()->message(),
            base::UintToString16(kNumExtraErrors));
  // ..and the last stored should be the 105th reported.
  ASSERT_EQ(errors.back()->message(),
            base::UintToString16(kMaxErrorsPerExtension + kNumExtraErrors - 1));
}

}  // namespace extensions
