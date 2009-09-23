// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/location_bar.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class AutocompleteBrowserTest : public InProcessBrowserTest {
 protected:
  LocationBar* GetLocationBar() const {
    return browser()->window()->GetLocationBar();
  }

  AutocompleteController* GetAutocompleteController() const {
    return GetLocationBar()->location_entry()->model()->popup_model()->
        autocomplete_controller();
  }
};

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, Basic) {
  LocationBar* location_bar = GetLocationBar();

  EXPECT_EQ(std::wstring(), location_bar->GetInputString());
  EXPECT_EQ(UTF8ToWide(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  // TODO(phajdan.jr): check state of IsSelectAll when it's consistent across
  // platforms.

  location_bar->FocusLocation();

  EXPECT_EQ(std::wstring(), location_bar->GetInputString());
  EXPECT_EQ(UTF8ToWide(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  EXPECT_TRUE(location_bar->location_entry()->IsSelectAll());

  location_bar->location_entry()->SetUserText(L"chrome");

  EXPECT_EQ(std::wstring(), location_bar->GetInputString());
  EXPECT_EQ(L"chrome", location_bar->location_entry()->GetText());
  EXPECT_FALSE(location_bar->location_entry()->IsSelectAll());

  location_bar->location_entry()->RevertAll();

  EXPECT_EQ(std::wstring(), location_bar->GetInputString());
  EXPECT_EQ(UTF8ToWide(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  EXPECT_FALSE(location_bar->location_entry()->IsSelectAll());

  location_bar->location_entry()->SetUserText(L"chrome");
  location_bar->Revert();

  EXPECT_EQ(std::wstring(), location_bar->GetInputString());
  EXPECT_EQ(UTF8ToWide(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  EXPECT_FALSE(location_bar->location_entry()->IsSelectAll());
}

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, Autocomplete) {
  LocationBar* location_bar = GetLocationBar();
  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  {
    autocomplete_controller->Start(L"chrome", std::wstring(),
                                   true, false, true);

    EXPECT_TRUE(autocomplete_controller->done());
    EXPECT_EQ(std::wstring(), location_bar->GetInputString());
    EXPECT_EQ(std::wstring(), location_bar->location_entry()->GetText());
    EXPECT_TRUE(location_bar->location_entry()->IsSelectAll());
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_EQ(1U, result.size());
    AutocompleteMatch match = result.match_at(0);
    EXPECT_EQ(AutocompleteMatch::SEARCH_WHAT_YOU_TYPED, match.type);
    EXPECT_FALSE(match.deletable);
  }

  {
    location_bar->Revert();

    EXPECT_EQ(std::wstring(), location_bar->GetInputString());
    EXPECT_EQ(UTF8ToWide(chrome::kAboutBlankURL),
              location_bar->location_entry()->GetText());
    EXPECT_FALSE(location_bar->location_entry()->IsSelectAll());
    const AutocompleteResult& result = autocomplete_controller->result();
    EXPECT_TRUE(result.empty());
  }
}
