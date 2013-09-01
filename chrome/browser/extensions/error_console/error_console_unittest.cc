// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_error.h"
#include "extensions/common/constants.h"
#include "extensions/common/id_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::string16;

namespace extensions {

namespace {

const char kDefaultStackTrace[] = "function_name (https://url.com:1:1)";

StackTrace GetDefaultStackTrace() {
  StackTrace stack_trace;
  scoped_ptr<StackFrame> frame =
      StackFrame::CreateFromText(base::UTF8ToUTF16(kDefaultStackTrace));
  CHECK(frame.get());
  stack_trace.push_back(*frame);
  return stack_trace;
}

string16 GetSourceForExtensionId(const std::string& extension_id) {
  return base::UTF8ToUTF16(
      std::string(kExtensionScheme) +
      content::kStandardSchemeSeparator +
      extension_id);
}

scoped_ptr<ExtensionError> CreateNewRuntimeError(
    bool from_incognito,
    const std::string& extension_id,
    const string16& message) {
  return scoped_ptr<ExtensionError>(new RuntimeError(
      from_incognito,
      GetSourceForExtensionId(extension_id),
      message,
      GetDefaultStackTrace(),
      GURL::EmptyGURL(),  // no context url
      logging::LOG_INFO));
}

}  // namespace

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

// Test adding errors, and removing them by reference, by incognito status,
// and in bulk.
TEST_F(ErrorConsoleUnitTest, AddAndRemoveErrors) {
  ASSERT_EQ(0u, error_console_->errors().size());

  const size_t kNumTotalErrors = 6;
  const size_t kNumNonIncognitoErrors = 3;
  const std::string kId = id_util::GenerateId("id");
  // Populate with both incognito and non-incognito errors (evenly distributed).
  for (size_t i = 0; i < kNumTotalErrors; ++i) {
    error_console_->ReportError(
        CreateNewRuntimeError(i % 2 == 0, kId, base::UintToString16(i)));
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
  const std::string kSecondId = id_util::GenerateId("id2");
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
  const std::string kId = id_util::GenerateId("id");

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
  ASSERT_EQ(base::UintToString16(kNumExtraErrors),
            errors.front()->message());
  // ..and the last stored should be the 105th reported.
  ASSERT_EQ(base::UintToString16(kMaxErrorsPerExtension + kNumExtraErrors - 1),
            errors.back()->message());
}

// Test to ensure that the error console will not add duplicate errors, but will
// keep the latest version of an error.
TEST_F(ErrorConsoleUnitTest, DuplicateErrorsAreReplaced) {
  ASSERT_EQ(0u, error_console_->errors().size());

  const std::string kId = id_util::GenerateId("id");
  const size_t kNumErrors = 3u;

  // Report three errors.
  for (size_t i = 0; i < kNumErrors; ++i) {
    error_console_->ReportError(
        CreateNewRuntimeError(false, kId, base::UintToString16(i)));
  }

  // Create an error identical to the second error reported, and save its
  // location.
  scoped_ptr<ExtensionError> runtime_error2 =
      CreateNewRuntimeError(false, kId, base::UintToString16(1u));
  const ExtensionError* weak_error = runtime_error2.get();

  // Add it to the error console.
  error_console_->ReportError(runtime_error2.Pass());

  // We should only have three errors stored, since two of the four reported
  // were identical, and the older should have been replaced.
  ASSERT_EQ(1u, error_console_->errors().size());
  const ErrorConsole::ErrorList& errors =
      error_console_->GetErrorsForExtension(kId);
  ASSERT_EQ(kNumErrors, errors.size());

  // The duplicate error should be the last reported (pointer comparison)...
  ASSERT_EQ(weak_error, errors.back());
  // ... and should have two reported occurrences.
  ASSERT_EQ(2u, errors.back()->occurrences());
}

}  // namespace extensions
