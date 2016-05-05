// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::string16 AutocompleteResultAsString(const AutocompleteResult& result) {
  std::string output(base::StringPrintf("{%" PRIuS "} ", result.size()));
  for (size_t i = 0; i < result.size(); ++i) {
    AutocompleteMatch match = result.match_at(i);
    output.append(base::StringPrintf("[\"%s\" by \"%s\"] ",
                                     base::UTF16ToUTF8(match.contents).c_str(),
                                     match.provider->GetName()));
  }
  return base::UTF8ToUTF16(output);
}

}  // namespace

class AutocompleteBrowserTest : public ExtensionBrowserTest {
 protected:
  void WaitForTemplateURLServiceToLoad() {
    search_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(browser()->profile()));
  }

  LocationBar* GetLocationBar() const {
    return browser()->window()->GetLocationBar();
  }

  AutocompleteController* GetAutocompleteController() const {
    return GetLocationBar()->GetOmniboxView()->model()->popup_model()->
        autocomplete_controller();
  }
};

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, Basic) {
  WaitForTemplateURLServiceToLoad();
  LocationBar* location_bar = GetLocationBar();
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();

  EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
  EXPECT_EQ(base::UTF8ToUTF16(url::kAboutBlankURL), omnibox_view->GetText());
  // TODO(phajdan.jr): check state of IsSelectAll when it's consistent across
  // platforms.

  location_bar->FocusLocation(true);

  EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
  EXPECT_EQ(base::UTF8ToUTF16(url::kAboutBlankURL), omnibox_view->GetText());
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  omnibox_view->SetUserText(base::ASCIIToUTF16("chrome"));

  EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
  EXPECT_EQ(base::ASCIIToUTF16("chrome"), omnibox_view->GetText());
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  omnibox_view->RevertAll();

  EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
  EXPECT_EQ(base::UTF8ToUTF16(url::kAboutBlankURL), omnibox_view->GetText());
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  omnibox_view->SetUserText(base::ASCIIToUTF16("chrome"));
  location_bar->Revert();

  EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
  EXPECT_EQ(base::UTF8ToUTF16(url::kAboutBlankURL), omnibox_view->GetText());
  EXPECT_FALSE(omnibox_view->IsSelectAll());
}

// Autocomplete test is flaky on ChromeOS.
// http://crbug.com/52928
#if defined(OS_CHROMEOS)
#define MAYBE_Autocomplete DISABLED_Autocomplete
#else
#define MAYBE_Autocomplete Autocomplete
#endif

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, MAYBE_Autocomplete) {
  WaitForTemplateURLServiceToLoad();
  // The results depend on the history backend being loaded. Make sure it is
  // loaded so that the autocomplete results are consistent.
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS));

  LocationBar* location_bar = GetLocationBar();
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  {
    omnibox_view->model()->SetInputInProgress(true);
    autocomplete_controller->Start(AutocompleteInput(
        base::ASCIIToUTF16("chrome"), base::string16::npos, std::string(),
        GURL(), metrics::OmniboxEventProto::NTP, true, false, true, false,
        false, ChromeAutocompleteSchemeClassifier(browser()->profile())));

    EXPECT_TRUE(autocomplete_controller->done());
    EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
    EXPECT_TRUE(omnibox_view->GetText().empty());
    EXPECT_TRUE(omnibox_view->IsSelectAll());
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_GE(result.size(), 1U) << AutocompleteResultAsString(result);
    AutocompleteMatch match = result.match_at(0);
    EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, match.type);
    EXPECT_FALSE(match.deletable);
  }

  {
    location_bar->Revert();
    EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
    EXPECT_EQ(base::UTF8ToUTF16(url::kAboutBlankURL), omnibox_view->GetText());
    EXPECT_FALSE(omnibox_view->IsSelectAll());
    const AutocompleteResult& result = autocomplete_controller->result();
    EXPECT_TRUE(result.empty()) << AutocompleteResultAsString(result);
  }
}

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, TabAwayRevertSelect) {
  WaitForTemplateURLServiceToLoad();
  // http://code.google.com/p/chromium/issues/detail?id=38385
  // Make sure that tabbing away from an empty omnibox causes a revert
  // and select all.
  LocationBar* location_bar = GetLocationBar();
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  EXPECT_EQ(base::UTF8ToUTF16(url::kAboutBlankURL), omnibox_view->GetText());
  omnibox_view->SetUserText(base::string16());
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser(),
                                GURL(url::kAboutBlankURL),
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  observer.Wait();
  EXPECT_EQ(base::UTF8ToUTF16(url::kAboutBlankURL), omnibox_view->GetText());
  chrome::CloseTab(browser());
  EXPECT_EQ(base::UTF8ToUTF16(url::kAboutBlankURL), omnibox_view->GetText());
  EXPECT_TRUE(omnibox_view->IsSelectAll());
}

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, FocusSearch) {
  WaitForTemplateURLServiceToLoad();
  LocationBar* location_bar = GetLocationBar();
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();

  // Focus search when omnibox is blank
  {
    EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
    EXPECT_EQ(base::UTF8ToUTF16(url::kAboutBlankURL), omnibox_view->GetText());

    location_bar->FocusSearch();
    EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
    EXPECT_EQ(base::ASCIIToUTF16("?"), omnibox_view->GetText());

    size_t selection_start, selection_end;
    omnibox_view->GetSelectionBounds(&selection_start, &selection_end);
    EXPECT_EQ(1U, selection_start);
    EXPECT_EQ(1U, selection_end);
  }

  // Focus search when omnibox is _not_ alread in forced query mode.
  {
    omnibox_view->SetUserText(base::ASCIIToUTF16("foo"));
    EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
    EXPECT_EQ(base::ASCIIToUTF16("foo"), omnibox_view->GetText());

    location_bar->FocusSearch();
    EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
    EXPECT_EQ(base::ASCIIToUTF16("?"), omnibox_view->GetText());

    size_t selection_start, selection_end;
    omnibox_view->GetSelectionBounds(&selection_start, &selection_end);
    EXPECT_EQ(1U, selection_start);
    EXPECT_EQ(1U, selection_end);
  }

  // Focus search when omnibox _is_ already in forced query mode, but no query
  // has been typed.
  {
    omnibox_view->SetUserText(base::ASCIIToUTF16("?"));
    EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
    EXPECT_EQ(base::ASCIIToUTF16("?"), omnibox_view->GetText());

    location_bar->FocusSearch();
    EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
    EXPECT_EQ(base::ASCIIToUTF16("?"), omnibox_view->GetText());

    size_t selection_start, selection_end;
    omnibox_view->GetSelectionBounds(&selection_start, &selection_end);
    EXPECT_EQ(1U, selection_start);
    EXPECT_EQ(1U, selection_end);
  }

  // Focus search when omnibox _is_ already in forced query mode, and some query
  // has been typed.
  {
    omnibox_view->SetUserText(base::ASCIIToUTF16("?foo"));
    EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
    EXPECT_EQ(base::ASCIIToUTF16("?foo"), omnibox_view->GetText());

    location_bar->FocusSearch();
    EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
    EXPECT_EQ(base::ASCIIToUTF16("?foo"), omnibox_view->GetText());

    size_t selection_start, selection_end;
    omnibox_view->GetSelectionBounds(&selection_start, &selection_end);
    EXPECT_EQ(1U, std::min(selection_start, selection_end));
    EXPECT_EQ(4U, std::max(selection_start, selection_end));
  }

  // Focus search when omnibox is in forced query mode with leading whitespace.
  {
    omnibox_view->SetUserText(base::ASCIIToUTF16("   ?foo"));
    EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
    EXPECT_EQ(base::ASCIIToUTF16("   ?foo"), omnibox_view->GetText());

    location_bar->FocusSearch();
    EXPECT_FALSE(location_bar->GetDestinationURL().is_valid());
    EXPECT_EQ(base::ASCIIToUTF16("   ?foo"), omnibox_view->GetText());

    size_t selection_start, selection_end;
    omnibox_view->GetSelectionBounds(&selection_start, &selection_end);
    EXPECT_EQ(4U, std::min(selection_start, selection_end));
    EXPECT_EQ(7U, std::max(selection_start, selection_end));
  }
}
