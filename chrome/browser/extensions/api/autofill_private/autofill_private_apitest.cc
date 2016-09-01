// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

class AutofillPrivateApiTest : public ExtensionApiTest {
 public:
  AutofillPrivateApiTest() {}
  ~AutofillPrivateApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();
  }

 protected:
  bool RunAutofillSubtest(const std::string& subtest) {
    return RunExtensionSubtest("autofill_private",
                               "main.html?" + subtest,
                               kFlagLoadAsComponent);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateApiTest);
};

}  // namespace

// TODO(hcarmona): Investigate converting these tests to unittests.

// TODO(crbug.com/643097) Disabled for flakiness.
#if defined(OS_WIN)
#define MAYBE_SaveAddress DISABLED_SaveAddress
#else
#define MAYBE_SaveAddress SaveAddress
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, MAYBE_SaveAddress) {
  EXPECT_TRUE(RunAutofillSubtest("saveAddress")) << message_;
}

// TODO(crbug.com/643097) Disabled for flakiness.
#if defined(OS_WIN)
#define MAYBE_GetCountryList DISABLED_GetCountryList
#else
#define MAYBE_GetCountryList GetCountryList
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, MAYBE_GetCountryList) {
  EXPECT_TRUE(RunAutofillSubtest("getCountryList")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, GetAddressComponents) {
  EXPECT_TRUE(RunAutofillSubtest("getAddressComponents")) << message_;
}

// TODO(crbug.com/643097) Disabled for flakiness.
#if defined(OS_WIN)
#define MAYBE_SaveCreditCard DISABLED_SaveCreditCard
#else
#define MAYBE_SaveCreditCard SaveCreditCard
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, MAYBE_SaveCreditCard) {
  EXPECT_TRUE(RunAutofillSubtest("saveCreditCard")) << message_;
}

// TODO(crbug.com/643097) Disabled for flakiness.
#if defined(OS_WIN)
#define MAYBE_RemoveEntry DISABLED_RemoveEntry
#else
#define MAYBE_RemoveEntry RemoveEntry
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, MAYBE_RemoveEntry) {
  EXPECT_TRUE(RunAutofillSubtest("removeEntry")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, ValidatePhoneNumbers) {
  EXPECT_TRUE(RunAutofillSubtest("ValidatePhoneNumbers")) << message_;
}

}  // namespace extensions

