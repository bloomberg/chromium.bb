// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Autocomplete test is flaky on ChromeOS.
// http://crbug.com/52928
#if defined(OS_CHROMEOS)
#define MAYBE_Autocomplete FLAKY_Autocomplete
#else
#define MAYBE_Autocomplete Autocomplete
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

  EXPECT_TRUE(location_bar->GetInputString().empty());
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  // TODO(phajdan.jr): check state of IsSelectAll when it's consistent across
  // platforms.

  location_bar->FocusLocation(true);

  EXPECT_TRUE(location_bar->GetInputString().empty());
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  EXPECT_TRUE(location_bar->location_entry()->IsSelectAll());

  location_bar->location_entry()->SetUserText(ASCIIToUTF16("chrome"));

  EXPECT_TRUE(location_bar->GetInputString().empty());
  EXPECT_EQ(ASCIIToUTF16("chrome"), location_bar->location_entry()->GetText());
  EXPECT_FALSE(location_bar->location_entry()->IsSelectAll());

  location_bar->location_entry()->RevertAll();

  EXPECT_TRUE(location_bar->GetInputString().empty());
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  EXPECT_FALSE(location_bar->location_entry()->IsSelectAll());

  location_bar->location_entry()->SetUserText(ASCIIToUTF16("chrome"));
  location_bar->Revert();

  EXPECT_TRUE(location_bar->GetInputString().empty());
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  EXPECT_FALSE(location_bar->location_entry()->IsSelectAll());
}

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, MAYBE_Autocomplete) {
  // The results depend on the history backend being loaded. Make sure it is
  // loaded so that the autocomplete results are consistent.
  ui_test_utils::WaitForHistoryToLoad(browser());

  LocationBar* location_bar = GetLocationBar();
  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  {
    autocomplete_controller->Start(ASCIIToUTF16("chrome"), string16(),
                                   true, false, true, true);

    EXPECT_TRUE(autocomplete_controller->done());
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_TRUE(location_bar->location_entry()->GetText().empty());
    EXPECT_TRUE(location_bar->location_entry()->IsSelectAll());
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_EQ(1U, result.size()) << AutocompleteResultAsString(result);
    AutocompleteMatch match = result.match_at(0);
    EXPECT_EQ(AutocompleteMatch::SEARCH_WHAT_YOU_TYPED, match.type);
    EXPECT_FALSE(match.deletable);
  }

  {
    location_bar->Revert();

    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL),
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
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  location_bar->location_entry()->SetUserText(string16());
  browser()->AddSelectedTabWithURL(GURL(chrome::kAboutBlankURL),
                                   PageTransition::START_PAGE);
  ui_test_utils::WaitForNavigation(
      &browser()->GetSelectedTabContents()->controller());
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  browser()->CloseTab();
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL),
            location_bar->location_entry()->GetText());
  EXPECT_TRUE(location_bar->location_entry()->IsSelectAll());
}

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, FocusSearch) {
  LocationBar* location_bar = GetLocationBar();

  // Focus search when omnibox is blank
  {
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL),
              location_bar->location_entry()->GetText());

    location_bar->FocusSearch();
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("?"), location_bar->location_entry()->GetText());

    size_t selection_start, selection_end;
    location_bar->location_entry()->GetSelectionBounds(&selection_start,
                                                       &selection_end);
    EXPECT_EQ(1U, selection_start);
    EXPECT_EQ(1U, selection_end);
  }

  // Focus search when omnibox is _not_ alread in forced query mode.
  {
    location_bar->location_entry()->SetUserText(ASCIIToUTF16("foo"));
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("foo"), location_bar->location_entry()->GetText());

    location_bar->FocusSearch();
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("?"), location_bar->location_entry()->GetText());

    size_t selection_start, selection_end;
    location_bar->location_entry()->GetSelectionBounds(&selection_start,
                                                       &selection_end);
    EXPECT_EQ(1U, selection_start);
    EXPECT_EQ(1U, selection_end);
  }

  // Focus search when omnibox _is_ already in forced query mode, but no query
  // has been typed.
  {
    location_bar->location_entry()->SetUserText(ASCIIToUTF16("?"));
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("?"), location_bar->location_entry()->GetText());

    location_bar->FocusSearch();
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("?"), location_bar->location_entry()->GetText());

    size_t selection_start, selection_end;
    location_bar->location_entry()->GetSelectionBounds(&selection_start,
                                                       &selection_end);
    EXPECT_EQ(1U, selection_start);
    EXPECT_EQ(1U, selection_end);
  }

  // Focus search when omnibox _is_ already in forced query mode, and some query
  // has been typed.
  {
    location_bar->location_entry()->SetUserText(ASCIIToUTF16("?foo"));
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("?foo"), location_bar->location_entry()->GetText());

    location_bar->FocusSearch();
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("?foo"), location_bar->location_entry()->GetText());

    size_t selection_start, selection_end;
    location_bar->location_entry()->GetSelectionBounds(&selection_start,
                                                       &selection_end);
    EXPECT_EQ(1U, std::min(selection_start, selection_end));
    EXPECT_EQ(4U, std::max(selection_start, selection_end));
  }

  // Focus search when omnibox is in forced query mode with leading whitespace.
  {
    location_bar->location_entry()->SetUserText(ASCIIToUTF16("   ?foo"));
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("   ?foo"),
              location_bar->location_entry()->GetText());

    location_bar->FocusSearch();
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("   ?foo"),
              location_bar->location_entry()->GetText());

    size_t selection_start, selection_end;
    location_bar->location_entry()->GetSelectionBounds(&selection_start,
                                                       &selection_end);
    EXPECT_EQ(4U, std::min(selection_start, selection_end));
    EXPECT_EQ(7U, std::max(selection_start, selection_end));
  }
}
