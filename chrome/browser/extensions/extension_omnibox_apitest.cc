// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"

// Basic test is flaky on ChromeOS.
// http://crbug.com/52929
#if defined(OS_CHROMEOS)
#define MAYBE_Basic FLAKY_Basic
#else
#define MAYBE_Basic Basic
#endif

namespace {

string16 AutocompleteResultAsString(const AutocompleteResult& result) {
  std::string output(base::StringPrintf("{%" PRIuS "} ", result.size()));
  for (size_t i = 0; i < result.size(); ++i) {
    AutocompleteMatch match = result.match_at(i);
    std::string provider_name = match.provider->name();
    output.append(base::StringPrintf("[\"%s\" by \"%s\"] ",
                                     UTF16ToUTF8(match.contents).c_str(),
                                     provider_name.c_str()));
  }
  return UTF8ToUTF16(output);
}

}  // namespace

class OmniboxApiTest : public ExtensionApiTest {
 protected:
  LocationBar* GetLocationBar() const {
    return browser()->window()->GetLocationBar();
  }

  AutocompleteController* GetAutocompleteController() const {
    return GetLocationBar()->location_entry()->model()->popup_model()->
        autocomplete_controller();
  }

  void WaitForTemplateURLModelToLoad() {
    TemplateURLModel* model =
        browser()->profile()->GetTemplateURLModel();
    model->Load();
    if (!model->loaded()) {
      ui_test_utils::WaitForNotification(
          NotificationType::TEMPLATE_URL_MODEL_LOADED);
    }
  }

  void WaitForAutocompleteDone(AutocompleteController* controller) {
    while (!controller->done()) {
      ui_test_utils::WaitForNotification(
          NotificationType::AUTOCOMPLETE_CONTROLLER_RESULT_UPDATED);
    }
  }
};

IN_PROC_BROWSER_TEST_F(OmniboxApiTest, MAYBE_Basic) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("omnibox")) << message_;

  // The results depend on the TemplateURLModel being loaded. Make sure it is
  // loaded so that the autocomplete results are consistent.
  WaitForTemplateURLModelToLoad();

  LocationBar* location_bar = GetLocationBar();
  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  // Test that our extension's keyword is suggested to us when we partially type
  // it.
  {
    autocomplete_controller->Start(ASCIIToUTF16("keywor"), string16(),
                                   true, false, true, false);

    WaitForAutocompleteDone(autocomplete_controller);
    EXPECT_TRUE(autocomplete_controller->done());
    EXPECT_EQ(std::wstring(), location_bar->GetInputString());
    EXPECT_EQ(string16(), location_bar->location_entry()->GetText());
    EXPECT_TRUE(location_bar->location_entry()->IsSelectAll());

    // First result should be to search for what was typed, second should be to
    // enter "extension keyword" mode.
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_EQ(2U, result.size()) << AutocompleteResultAsString(result);
    AutocompleteMatch match = result.match_at(0);
    EXPECT_EQ(AutocompleteMatch::SEARCH_WHAT_YOU_TYPED, match.type);
    EXPECT_FALSE(match.deletable);

    match = result.match_at(1);
    ASSERT_TRUE(match.template_url);
    EXPECT_TRUE(match.template_url->IsExtensionKeyword());
    EXPECT_EQ(ASCIIToUTF16("keyword"), match.template_url->keyword());
  }

  // Test that our extension can send suggestions back to us.
  {
    autocomplete_controller->Start(ASCIIToUTF16("keyword suggestio"),
                                   string16(), true, false, true, false);

    WaitForAutocompleteDone(autocomplete_controller);
    EXPECT_TRUE(autocomplete_controller->done());

    // First result should be to invoke the keyword with what we typed, 2-4
    // should be to invoke with suggestions from the extension, and the last
    // should be to search for what we typed.
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_EQ(5U, result.size()) << AutocompleteResultAsString(result);

    ASSERT_TRUE(result.match_at(0).template_url);
    EXPECT_EQ(ASCIIToUTF16("keyword suggestio"),
              result.match_at(0).fill_into_edit);
    EXPECT_EQ(ASCIIToUTF16("keyword suggestion1"),
              result.match_at(1).fill_into_edit);
    EXPECT_EQ(ASCIIToUTF16("keyword suggestion2"),
              result.match_at(2).fill_into_edit);
    EXPECT_EQ(ASCIIToUTF16("keyword suggestion3"),
              result.match_at(3).fill_into_edit);

    string16 description =
        ASCIIToUTF16("Description with style: <match>, [dim], (url till end)");
    EXPECT_EQ(description, result.match_at(1).contents);
    ASSERT_EQ(6u, result.match_at(1).contents_class.size());

    EXPECT_EQ(0u,
              result.match_at(1).contents_class[0].offset);
    EXPECT_EQ(ACMatchClassification::NONE,
              result.match_at(1).contents_class[0].style);

    EXPECT_EQ(description.find('<'),
              result.match_at(1).contents_class[1].offset);
    EXPECT_EQ(ACMatchClassification::MATCH,
              result.match_at(1).contents_class[1].style);

    EXPECT_EQ(description.find('>') + 1u,
              result.match_at(1).contents_class[2].offset);
    EXPECT_EQ(ACMatchClassification::NONE,
              result.match_at(1).contents_class[2].style);

    EXPECT_EQ(description.find('['),
              result.match_at(1).contents_class[3].offset);
    EXPECT_EQ(ACMatchClassification::DIM,
              result.match_at(1).contents_class[3].style);

    EXPECT_EQ(description.find(']') + 1u,
              result.match_at(1).contents_class[4].offset);
    EXPECT_EQ(ACMatchClassification::NONE,
              result.match_at(1).contents_class[4].style);

    EXPECT_EQ(description.find('('),
              result.match_at(1).contents_class[5].offset);
    EXPECT_EQ(ACMatchClassification::URL,
              result.match_at(1).contents_class[5].style);

    AutocompleteMatch match = result.match_at(4);
    EXPECT_EQ(AutocompleteMatch::SEARCH_WHAT_YOU_TYPED, match.type);
    EXPECT_FALSE(match.deletable);
  }

  {
    ResultCatcher catcher;
    autocomplete_controller->Start(ASCIIToUTF16("keyword command"), string16(),
                                   true, false, true, false);
    location_bar->AcceptInput();
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}
