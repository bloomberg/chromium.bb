// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_driven_test.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "ios/chrome/browser/autofill/autofill_controller.h"
#import "ios/chrome/browser/autofill/form_suggestion_controller.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

namespace {

const base::FilePath::CharType kTestName[] = FILE_PATH_LITERAL("heuristics");

const base::FilePath& GetTestDataDir() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, dir, ());
  if (dir.empty())
    PathService::Get(ios::DIR_TEST_DATA, &dir);
  return dir;
}

base::FilePath GetIOSOutputDirectory(
    const base::FilePath::StringType& test_name) {
  base::FilePath dir;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &dir));

  dir = dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("autofill")
            .AppendASCII("heuristics")
            .AppendASCII("output");

  return dir;
}

// Returns the list of tests that fail to run properly on iOS.
// TODO(crbug.com/762970): Reduce the amount of failing tests.
const std::set<std::string>& FailingTestData() {
  static std::set<std::string> failing_tests = {
      "016_misc_phones.html",
      "017_checkout_advanceautoparts.com.html",
      "018_checkout_ae.com.html",
      "020_checkout_cafepress.com.html",
      "025_checkout_hsn.com.html",
      "028_checkout_jr.com.html",
      "030_checkout_lowes.com.html",
      "031_checkout_macys.com.html",
      "034_checkout_overstock.com.html",
      "040_checkout_urbanoutfitters.com.html",
      "049_register_ebay.com.html",
      "059_register_macys.com.html",
      "062_register_newegg.com.html",
      "063_register_officedepot.com.html",
      "066_register_rediff.com.html",
      "079_crbug_52198.html",
      "086_crbug_98269.html",
      "098_register_epson.com.mx.html",
      "102_checkout_m_macys.com.html",
      "103_checkout_peapod.com.html",
      "110_checkout_harryanddavid.com.html",
      "113_guest_checkout_peapod.com.html",
      "115_checkout_walgreens.com.html",
      "116_cc_checkout_walgreens.com.html",
      "130_bug_465576.html",
      "131_bug_465587.html",
      "137_bug_555010.html",
      "144_cc_checkout_m_jcp.com.html",
      "146_checkout_store.scholastic.com.html",
  };
  return failing_tests;
}

const std::vector<base::FilePath> GetTestFiles() {
  base::FilePath dir;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &dir));
  dir = dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("autofill")
            .AppendASCII("heuristics")
            .AppendASCII("input");

  base::FileEnumerator input_files(dir, false, base::FileEnumerator::FILES);
  std::vector<base::FilePath> files;
  for (base::FilePath input_file = input_files.Next(); !input_file.empty();
       input_file = input_files.Next()) {
    files.push_back(input_file);
  }
  std::sort(files.begin(), files.end());

  base::mac::ClearAmIBundledCache();
  return files;
}

}  // namespace

// Test fixture for verifying Autofill heuristics. Each input is an HTML
// file that contains one or more forms. The corresponding output file lists the
// heuristically detected type for each field.
// This is based on FormStructureBrowserTest from the Chromium Project.
// TODO(crbug.com/245246): Unify the tests.
class FormStructureBrowserTest
    : public ChromeWebTest,
      public DataDrivenTest,
      public ::testing::WithParamInterface<base::FilePath> {
 protected:
  FormStructureBrowserTest();
  ~FormStructureBrowserTest() override {}

  void SetUp() override;
  void TearDown() override;

  // DataDrivenTest:
  void GenerateResults(const std::string& input, std::string* output) override;

  // Serializes the given |forms| into a string.
  std::string FormStructuresToString(
      const std::vector<std::unique_ptr<FormStructure>>& forms);

  FormSuggestionController* suggestionController_;
  AutofillController* autofillController_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormStructureBrowserTest);
};

FormStructureBrowserTest::FormStructureBrowserTest()
    : DataDrivenTest(GetTestDataDir()) {}

void FormStructureBrowserTest::SetUp() {
  ChromeWebTest::SetUp();

  InfoBarManagerImpl::CreateForWebState(web_state());
  AutofillAgent* autofillAgent = [[AutofillAgent alloc]
      initWithPrefService:chrome_browser_state_->GetPrefs()
                 webState:web_state()];
  suggestionController_ =
      [[FormSuggestionController alloc] initWithWebState:web_state()
                                               providers:@[ autofillAgent ]];
  autofillController_ = [[AutofillController alloc]
           initWithBrowserState:chrome_browser_state_.get()
                       webState:web_state()
                  autofillAgent:autofillAgent
      passwordGenerationManager:nullptr
                downloadEnabled:NO];
}

void FormStructureBrowserTest::TearDown() {
  [autofillController_ detachFromWebState];

  ChromeWebTest::TearDown();
}

void FormStructureBrowserTest::GenerateResults(const std::string& input,
                                               std::string* output) {
  LoadHtml(input);
  AutofillManager* autofill_manager =
      AutofillDriverIOS::FromWebState(web_state())->autofill_manager();
  ASSERT_NE(nullptr, autofill_manager);
  const std::vector<std::unique_ptr<FormStructure>>& forms =
      autofill_manager->form_structures_;
  *output = FormStructureBrowserTest::FormStructuresToString(forms);
}

std::string FormStructureBrowserTest::FormStructuresToString(
    const std::vector<std::unique_ptr<FormStructure>>& forms) {
  std::string forms_string;
  for (const std::unique_ptr<FormStructure>& form : forms) {
    for (const auto& field : *form) {
      forms_string += field->Type().ToString();
      forms_string += " | " + base::UTF16ToUTF8(field->name);
      forms_string += " | " + base::UTF16ToUTF8(field->label);
      forms_string += " | " + base::UTF16ToUTF8(field->value);
      forms_string += " | " + field->section();
      forms_string += "\n";
    }
  }
  return forms_string;
}

TEST_P(FormStructureBrowserTest, DataDrivenHeuristics) {
  auto param = GetParam();
  auto failing_tests = FailingTestData();
  bool test_expected_to_fail =
      base::ContainsKey(failing_tests, param.BaseName().value());
  DataDrivenTestStatus test_status =
      test_expected_to_fail ? TEST_FAILING : TEST_PASSING;
  RunOneDataDrivenTest(GetParam(), GetIOSOutputDirectory(kTestName),
                       test_status);
}

INSTANTIATE_TEST_CASE_P(AllForms,
                        FormStructureBrowserTest,
                        testing::ValuesIn(GetTestFiles()));

}  // namespace autofill
