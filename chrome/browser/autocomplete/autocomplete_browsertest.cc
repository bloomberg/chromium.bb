// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Basic test is crashy on Mac
// http://crbug.com/49324
#if defined(OS_MAC)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif

namespace {

std::wstring AutocompleteResultAsString(const AutocompleteResult& result) {
  std::wstring output(StringPrintf(L"{%d} ", result.size()));
  for (size_t i = 0; i < result.size(); ++i) {
    AutocompleteMatch match = result.match_at(i);
    std::wstring provider_name(ASCIIToWide(match.provider->name()));
    output.append(StringPrintf(L"[\"%ls\" by \"%ls\"] ",
                               match.contents.c_str(),
                               provider_name.c_str()));
  }
  return output;
}

}  // namespace

class AutocompleteBrowserTest : public InProcessBrowserTest {
 protected:
  LocationBar* GetLocationBar() const {
    return browser()->window()->GetLocationBar();
  }

  AutocompleteController* GetAutocompleteController() const {
    return GetLocationBar()->location_entry()->model()->popup_model()->
        autocomplete_controller();
  }

  void WaitForHistoryBackendToLoad() {
    HistoryService* history_service =
        browser()->profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (!history_service->BackendLoaded())
      ui_test_utils::WaitForNotification(NotificationType::HISTORY_LOADED);
  }
};

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, MAYBE_Basic) {
  LocationBar* location_bar = GetLocationBar();

  EXPECT_EQ(std::wstring(), location_bar->GetInputString());
  EXPECT_EQ(UTF8ToWide(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  // TODO(phajdan.jr): check state of IsSelectAll when it's consistent across
  // platforms.

  location_bar->FocusLocation(true);

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
  // The results depend on the history backend being loaded. Make sure it is
  // loaded so that the autocomplete results are consistent.
  WaitForHistoryBackendToLoad();

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
    ASSERT_EQ(1U, result.size()) << AutocompleteResultAsString(result);
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
    EXPECT_TRUE(result.empty()) << AutocompleteResultAsString(result);
  }
}

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, TabAwayRevertSelect) {
  // http://code.google.com/p/chromium/issues/detail?id=38385
  // Make sure that tabbing away from an empty omnibar causes a revert
  // and select all.
  LocationBar* location_bar = GetLocationBar();
  EXPECT_EQ(UTF8ToWide(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  location_bar->location_entry()->SetUserText(L"");
  browser()->AddTabWithURL(
      GURL(chrome::kAboutBlankURL), GURL(), PageTransition::START_PAGE,
      -1, TabStripModel::ADD_SELECTED, NULL, std::string());
  ui_test_utils::WaitForNavigation(
      &browser()->GetSelectedTabContents()->controller());
  EXPECT_EQ(UTF8ToWide(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  browser()->CloseTab();
  EXPECT_EQ(UTF8ToWide(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  EXPECT_TRUE(location_bar->location_entry()->IsSelectAll());
}
