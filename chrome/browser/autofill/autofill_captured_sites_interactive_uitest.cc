// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/captured_sites_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_manager_test_delegate.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/state_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const base::TimeDelta autofill_wait_for_action_interval =
    base::TimeDelta::FromSeconds(5);

base::FilePath GetReplayFilesDirectory() {
  base::FilePath src_dir;
  if (base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
    return src_dir.Append(
        FILE_PATH_LITERAL("chrome/test/data/autofill/captured_sites"));
  } else {
    src_dir.clear();
    return src_dir;
  }
}

// Iterate through Autofill's Web Page Replay capture file directory to look
// for captures sites and automation recipe files. Return a list of sites for
// which recipe-based testing is available.
std::vector<std::string> GetCapturedSites() {
  std::vector<std::string> sites;
  base::FileEnumerator capture_files(GetReplayFilesDirectory(), false,
                                     base::FileEnumerator::FILES);
  for (base::FilePath file = capture_files.Next(); !file.empty();
       file = capture_files.Next()) {
    // If a site capture file is found, also look to see if the directory has
    // a corresponding recorded action recipe log file.
    // A site capture file has no extension. A recorded action recipe log file
    // has the '.test' extension.
    if (file.Extension().empty() &&
        base::PathExists(file.AddExtension(FILE_PATH_LITERAL(".test")))) {
      sites.push_back(
          captured_sites_test_utils::FilePathToUTF8(file.BaseName().value()));
    }
  }
  std::sort(sites.begin(), sites.end());
  return sites;
}

}  // namespace

namespace autofill {

class AutofillCapturedSitesInteractiveTest
    : public AutofillUiTest,
      public captured_sites_test_utils::
          TestRecipeReplayChromeFeatureActionExecutor,
      public ::testing::WithParamInterface<std::string> {
 public:
  // TestRecipeReplayChromeFeatureActionExecutor
  bool AutofillForm(content::RenderFrameHost* frame,
                    const std::string& focus_element_css_selector,
                    const int attempts = 1) override {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(frame);
    AutofillManager* autofill_manager =
        ContentAutofillDriverFactory::FromWebContents(web_contents)
            ->DriverForFrame(frame)
            ->autofill_manager();
    autofill_manager->SetTestDelegate(test_delegate());

    int tries = 0;
    while (tries < attempts) {
      tries++;
      autofill_manager->client()->HideAutofillPopup();

      if (!ShowAutofillSuggestion(frame, focus_element_css_selector)) {
        LOG(WARNING) << "Failed to bring up the autofill suggestion drop down.";
        continue;
      }

      // Press the down key to highlight the first choice in the autofill
      // suggestion drop down.
      test_delegate()->Reset();
      SendKeyToPopup(frame, ui::DomKey::ARROW_DOWN);
      if (!test_delegate()->Wait({ObservedUiEvents::kPreviewFormData},
                                 autofill_wait_for_action_interval)) {
        LOG(WARNING) << "Failed to select an option from the "
                     << "autofill suggestion drop down.";
        continue;
      }

      // Press the enter key to invoke autofill using the first suggestion.
      test_delegate()->Reset();
      SendKeyToPopup(frame, ui::DomKey::ENTER);
      if (!test_delegate()->Wait({ObservedUiEvents::kFormDataFilled},
                                 autofill_wait_for_action_interval)) {
        LOG(WARNING) << "Failed to fill the form.";
        continue;
      }

      return true;
    }

    autofill_manager->client()->HideAutofillPopup();
    ADD_FAILURE() << "Failed to autofill the form!";
    return false;
  }

 protected:
  AutofillCapturedSitesInteractiveTest()
      : profile_(test::GetFullProfile()),
        card_(CreditCard(base::GenerateGUID(), "http://www.example.com")) {}

  ~AutofillCapturedSitesInteractiveTest() override {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    AutofillUiTest::SetUpOnMainThread();
    SetupTestProfile();
    recipe_replayer_ =
        std::make_unique<captured_sites_test_utils::TestRecipeReplayer>(
            browser(), this);
    recipe_replayer()->Setup();
  }

  void TearDownOnMainThread() override {
    recipe_replayer()->Cleanup();
    AutofillUiTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable the autofill show typed prediction feature. When active this
    // feature forces input elements on a form to display their autofill type
    // prediction. Test will check this attribute on all the relevant input
    // elements in a form to determine if the form is ready for interaction.
    feature_list_.InitAndEnableFeature(features::kAutofillShowTypePredictions);
    command_line->AppendSwitch(switches::kShowAutofillTypePredictions);
    captured_sites_test_utils::TestRecipeReplayer::SetUpCommandLine(
        command_line);
  }

  captured_sites_test_utils::TestRecipeReplayer* recipe_replayer() {
    return recipe_replayer_.get();
  }

  const CreditCard credit_card() { return card_; }

  const AutofillProfile profile() { return profile_; }

 private:
  void SetupTestProfile() {
    test::SetCreditCardInfo(&card_, "Milton Waddams", "9621327911759602", "5",
                            "2027", "1");
    test::SetProfileInfo(&profile_, "Milton", "C.", "Waddams",
                         "red.swingline@initech.com", "Initech",
                         "4120 Freidrich Lane", "Apt 8", "Austin", "Texas",
                         "78744", "US", "5125551234");
    AddTestAutofillData(browser(), profile_, card_);
  }

  bool ShowAutofillSuggestion(content::RenderFrameHost* frame,
                              const std::string& target_element_xpath) {
    // First, automation should focus on the frame containg the autofill form.
    // Doing so ensures that Chrome scrolls the element into view if the
    // element is off the page.
    if (!captured_sites_test_utils::TestRecipeReplayer::PlaceFocusOnElement(
            frame, target_element_xpath))
      return false;

    int x, y;
    if (!captured_sites_test_utils::TestRecipeReplayer::
            GetCenterCoordinateOfTargetElement(frame, target_element_xpath, x,
                                               y))
      return false;

    test_delegate()->Reset();
    if (!captured_sites_test_utils::TestRecipeReplayer::
            SimulateLeftMouseClickAt(frame, gfx::Point(x, y)))
      return false;

    return test_delegate()->Wait({ObservedUiEvents::kSuggestionShown},
                                 autofill_wait_for_action_interval);
  }

  AutofillProfile profile_;
  CreditCard card_;
  std::unique_ptr<captured_sites_test_utils::TestRecipeReplayer>
      recipe_replayer_;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(AutofillCapturedSitesInteractiveTest, Recipe) {
  // Prints the path of the test to be executed.
  VLOG(1) << GetParam();

  // Craft the capture file path.
  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
  base::FilePath capture_file_path =
      GetReplayFilesDirectory().AppendASCII(GetParam().c_str());

  // Craft the recipe file path.
  base::FilePath recipe_file_path = GetReplayFilesDirectory().AppendASCII(
      base::StringPrintf("%s.test", GetParam().c_str()));

  ASSERT_TRUE(
      recipe_replayer()->ReplayTest(capture_file_path, recipe_file_path));
}

struct GetParamAsString {
  template <class ParamType>
  std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
    return info.param;
  }
};

INSTANTIATE_TEST_CASE_P(,
                        AutofillCapturedSitesInteractiveTest,
                        testing::ValuesIn(GetCapturedSites()),
                        GetParamAsString());
}  // namespace autofill
