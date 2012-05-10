// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/format_macros.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class AutocompleteBrowserTest : public ExtensionBrowserTest {
 protected:
  LocationBar* GetLocationBar() const {
    return browser()->window()->GetLocationBar();
  }

  AutocompleteController* GetAutocompleteController() const {
    return GetLocationBar()->GetLocationEntry()->model()->popup_model()->
        autocomplete_controller();
  }
};

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, Basic) {
  LocationBar* location_bar = GetLocationBar();
  OmniboxView* location_entry = location_bar->GetLocationEntry();

  EXPECT_TRUE(location_bar->GetInputString().empty());
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL), location_entry->GetText());
  // TODO(phajdan.jr): check state of IsSelectAll when it's consistent across
  // platforms.

  location_bar->FocusLocation(true);

  EXPECT_TRUE(location_bar->GetInputString().empty());
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL), location_entry->GetText());
  EXPECT_TRUE(location_entry->IsSelectAll());

  location_entry->SetUserText(ASCIIToUTF16("chrome"));

  EXPECT_TRUE(location_bar->GetInputString().empty());
  EXPECT_EQ(ASCIIToUTF16("chrome"), location_entry->GetText());
  EXPECT_FALSE(location_entry->IsSelectAll());

  location_entry->RevertAll();

  EXPECT_TRUE(location_bar->GetInputString().empty());
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL), location_entry->GetText());
  EXPECT_FALSE(location_entry->IsSelectAll());

  location_entry->SetUserText(ASCIIToUTF16("chrome"));
  location_bar->Revert();

  EXPECT_TRUE(location_bar->GetInputString().empty());
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL), location_entry->GetText());
  EXPECT_FALSE(location_entry->IsSelectAll());
}

// Autocomplete test is flaky on ChromeOS.
// http://crbug.com/52928
#if defined(OS_CHROMEOS)
#define MAYBE_Autocomplete DISABLED_Autocomplete
#else
#define MAYBE_Autocomplete Autocomplete
#endif

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, MAYBE_Autocomplete) {
  // The results depend on the history backend being loaded. Make sure it is
  // loaded so that the autocomplete results are consistent.
  ui_test_utils::WaitForHistoryToLoad(browser());

  LocationBar* location_bar = GetLocationBar();
  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  {
    autocomplete_controller->Start(
        ASCIIToUTF16("chrome"), string16(), true, false, true,
        AutocompleteInput::SYNCHRONOUS_MATCHES);

    OmniboxView* location_entry = location_bar->GetLocationEntry();

    EXPECT_TRUE(autocomplete_controller->done());
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_TRUE(location_entry->GetText().empty());
    EXPECT_TRUE(location_entry->IsSelectAll());
    const AutocompleteResult& result = autocomplete_controller->result();
    // We get two matches because we have a provider for extension apps and the
    // Chrome Web Store is a built-in Extension app. For this test, we only care
    // about the other match existing.
    ASSERT_GE(result.size(), 1U) << AutocompleteResultAsString(result);
    AutocompleteMatch match = result.match_at(0);
    EXPECT_EQ(AutocompleteMatch::SEARCH_WHAT_YOU_TYPED, match.type);
    EXPECT_FALSE(match.deletable);
  }

  {
    location_bar->Revert();
    OmniboxView* location_entry = location_bar->GetLocationEntry();

    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL), location_entry->GetText());
    EXPECT_FALSE(location_entry->IsSelectAll());
    const AutocompleteResult& result = autocomplete_controller->result();
    EXPECT_TRUE(result.empty()) << AutocompleteResultAsString(result);
  }
}

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, TabAwayRevertSelect) {
  // http://code.google.com/p/chromium/issues/detail?id=38385
  // Make sure that tabbing away from an empty omnibar causes a revert
  // and select all.
  LocationBar* location_bar = GetLocationBar();
  OmniboxView* location_entry = location_bar->GetLocationEntry();
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL), location_entry->GetText());
  location_entry->SetUserText(string16());
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  browser()->AddSelectedTabWithURL(GURL(chrome::kAboutBlankURL),
                                   content::PAGE_TRANSITION_START_PAGE);
  observer.Wait();
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL), location_entry->GetText());
  browser()->CloseTab();
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL), location_entry->GetText());
  EXPECT_TRUE(location_entry->IsSelectAll());
}

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, FocusSearch) {
  LocationBar* location_bar = GetLocationBar();
  OmniboxView* location_entry = location_bar->GetLocationEntry();

  // Focus search when omnibox is blank
  {
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL), location_entry->GetText());

    location_bar->FocusSearch();
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("?"), location_entry->GetText());

    size_t selection_start, selection_end;
    location_entry->GetSelectionBounds(&selection_start, &selection_end);
    EXPECT_EQ(1U, selection_start);
    EXPECT_EQ(1U, selection_end);
  }

  // Focus search when omnibox is _not_ alread in forced query mode.
  {
    location_entry->SetUserText(ASCIIToUTF16("foo"));
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("foo"), location_entry->GetText());

    location_bar->FocusSearch();
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("?"), location_entry->GetText());

    size_t selection_start, selection_end;
    location_entry->GetSelectionBounds(&selection_start, &selection_end);
    EXPECT_EQ(1U, selection_start);
    EXPECT_EQ(1U, selection_end);
  }

  // Focus search when omnibox _is_ already in forced query mode, but no query
  // has been typed.
  {
    location_entry->SetUserText(ASCIIToUTF16("?"));
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("?"), location_entry->GetText());

    location_bar->FocusSearch();
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("?"), location_entry->GetText());

    size_t selection_start, selection_end;
    location_entry->GetSelectionBounds(&selection_start, &selection_end);
    EXPECT_EQ(1U, selection_start);
    EXPECT_EQ(1U, selection_end);
  }

  // Focus search when omnibox _is_ already in forced query mode, and some query
  // has been typed.
  {
    location_entry->SetUserText(ASCIIToUTF16("?foo"));
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("?foo"), location_entry->GetText());

    location_bar->FocusSearch();
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("?foo"), location_entry->GetText());

    size_t selection_start, selection_end;
    location_entry->GetSelectionBounds(&selection_start, &selection_end);
    EXPECT_EQ(1U, std::min(selection_start, selection_end));
    EXPECT_EQ(4U, std::max(selection_start, selection_end));
  }

  // Focus search when omnibox is in forced query mode with leading whitespace.
  {
    location_entry->SetUserText(ASCIIToUTF16("   ?foo"));
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("   ?foo"), location_entry->GetText());

    location_bar->FocusSearch();
    EXPECT_TRUE(location_bar->GetInputString().empty());
    EXPECT_EQ(ASCIIToUTF16("   ?foo"), location_entry->GetText());

    size_t selection_start, selection_end;
    location_entry->GetSelectionBounds(&selection_start, &selection_end);
    EXPECT_EQ(4U, std::min(selection_start, selection_end));
    EXPECT_EQ(7U, std::max(selection_start, selection_end));
  }
}

IN_PROC_BROWSER_TEST_F(AutocompleteBrowserTest, ExtensionAppProvider) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  size_t extension_count = service->extensions()->size();

  FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  // Load a packaged app.
  extensions::UnpackedInstaller::Create(service)->Load(
      test_dir.AppendASCII("extensions").AppendASCII("packaged_app"));
  WaitForExtensionLoad();
  // Load a hosted app.
  extensions::UnpackedInstaller::Create(service)->Load(
      test_dir.AppendASCII("extensions").AppendASCII("app"));
  WaitForExtensionLoad();
  ASSERT_EQ(extension_count + 2U, service->extensions()->size());

  // The results depend on the history backend being loaded. Make sure it is
  // loaded so that the autocomplete results are consistent.
  ui_test_utils::WaitForHistoryToLoad(browser());

  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  // Try out the packaged app.
  {
    autocomplete_controller->Start(
        ASCIIToUTF16("Packaged App Test"), string16(), true, false, true,
        AutocompleteInput::SYNCHRONOUS_MATCHES);

    EXPECT_TRUE(autocomplete_controller->done());
    const AutocompleteResult& result = autocomplete_controller->result();
    EXPECT_GT(result.size(), 1U) << AutocompleteResultAsString(result);
    AutocompleteMatch match = result.match_at(0);
    EXPECT_EQ(ASCIIToUTF16("Packaged App Test"), match.contents);
    EXPECT_EQ(AutocompleteMatch::EXTENSION_APP, match.type);
    EXPECT_FALSE(match.deletable);
  }

  browser()->NewTab();

  // Try out the hosted app.
  {
    autocomplete_controller->Start(
        ASCIIToUTF16("App Test"), string16(), true, false, true,
        AutocompleteInput::SYNCHRONOUS_MATCHES);

    EXPECT_TRUE(autocomplete_controller->done());
    const AutocompleteResult& result = autocomplete_controller->result();
    // 'App test' is also a substring of extension 'Packaged App Test'.
    EXPECT_GT(result.size(), 2U) << AutocompleteResultAsString(result);
    AutocompleteMatch match = result.match_at(0);
    EXPECT_EQ(ASCIIToUTF16("App Test"), match.contents);
    EXPECT_EQ(AutocompleteMatch::EXTENSION_APP, match.type);
    EXPECT_FALSE(match.deletable);
  }
}
