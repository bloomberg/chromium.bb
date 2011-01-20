// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/message_loop.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "views/event.h"

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#endif

using base::Time;
using base::TimeDelta;

namespace {

const char kSearchKeyword[] = "foo";
const wchar_t kSearchKeywordKeys[] = {
  ui::VKEY_F, ui::VKEY_O, ui::VKEY_O, 0
};
const char kSearchURL[] = "http://www.foo.com/search?q={searchTerms}";
const char kSearchShortName[] = "foo";
const char kSearchText[] = "abc";
const wchar_t kSearchTextKeys[] = {
  ui::VKEY_A, ui::VKEY_B, ui::VKEY_C, 0
};
const char kSearchTextURL[] = "http://www.foo.com/search?q=abc";
const char kSearchSingleChar[] = "z";
const wchar_t kSearchSingleCharKeys[] = { ui::VKEY_Z, 0 };
const char kSearchSingleCharURL[] = "http://www.foo.com/search?q=z";

const char kHistoryPageURL[] = "chrome://history/#q=abc";

const char kDesiredTLDHostname[] = "www.bar.com";
const wchar_t kDesiredTLDKeys[] = {
  ui::VKEY_B, ui::VKEY_A, ui::VKEY_R, 0
};

// Hostnames that shall be blocked by host resolver.
const char *kBlockedHostnames[] = {
  "foo",
  "*.foo.com",
  "bar",
  "*.bar.com",
  "abc",
  "*.abc.com",
  "history",
  "z"
};

const struct TestHistoryEntry {
  const char* url;
  const char* title;
  const char* body;
  int visit_count;
  int typed_count;
  bool starred;
} kHistoryEntries[] = {
  {"http://www.bar.com/1", "Page 1", kSearchText, 1, 1, false },
  {"http://www.bar.com/2", "Page 2", kSearchText, 1, 1, false },
  {"http://www.bar.com/3", "Page 3", kSearchText, 1, 1, false },
  {"http://www.bar.com/4", "Page 4", kSearchText, 1, 1, false },
  {"http://www.bar.com/5", "Page 5", kSearchText, 1, 1, false },
  {"http://www.bar.com/6", "Page 6", kSearchText, 1, 1, false },
  {"http://www.bar.com/7", "Page 7", kSearchText, 1, 1, false },
  {"http://www.bar.com/8", "Page 8", kSearchText, 1, 1, false },
  {"http://www.bar.com/9", "Page 9", kSearchText, 1, 1, false },

  // To trigger inline autocomplete.
  {"http://www.abc.com", "Page abc", kSearchText, 10000, 10000, true },
};

#if defined(OS_LINUX)
// Returns the text stored in the PRIMARY clipboard.
std::string GetPrimarySelectionText() {
  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  DCHECK(clipboard);

  gchar* selection_text = gtk_clipboard_wait_for_text(clipboard);
  std::string result(selection_text ? selection_text : "");
  g_free(selection_text);
  return result;
}

// Stores the given text to clipboard.
void SetClipboardText(const char* text) {
  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  DCHECK(clipboard);

  gtk_clipboard_set_text(clipboard, text, -1);
}
#endif

}  // namespace

class AutocompleteEditViewTest : public InProcessBrowserTest,
                                 public NotificationObserver {
 protected:
  AutocompleteEditViewTest() {
    set_show_window(true);
  }

  static void GetAutocompleteEditViewForBrowser(
      const Browser* browser,
      AutocompleteEditView** edit_view) {
    BrowserWindow* window = browser->window();
    ASSERT_TRUE(window);
    LocationBar* loc_bar = window->GetLocationBar();
    ASSERT_TRUE(loc_bar);
    *edit_view = loc_bar->location_entry();
    ASSERT_TRUE(*edit_view);
  }

  void GetAutocompleteEditView(AutocompleteEditView** edit_view) {
    GetAutocompleteEditViewForBrowser(browser(), edit_view);
  }

  static void SendKeyForBrowser(const Browser* browser,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser, key, control, shift, alt, false /* command */));
  }

  void SendKey(ui::KeyboardCode key, bool control, bool shift, bool alt) {
    SendKeyForBrowser(browser(), key, control, shift, alt);
  }

  void SendKeySequence(const wchar_t* keys) {
    for (; *keys; ++keys)
      ASSERT_NO_FATAL_FAILURE(SendKey(static_cast<ui::KeyboardCode>(*keys),
                                      false, false, false));
  }

  void WaitForTabOpenOrCloseForBrowser(const Browser* browser,
                                       int expected_tab_count) {
    int tab_count = browser->tab_count();
    if (tab_count == expected_tab_count)
      return;

    NotificationRegistrar registrar;
    registrar.Add(this,
                  (tab_count < expected_tab_count ?
                   NotificationType::TAB_PARENTED :
                   NotificationType::TAB_CLOSED),
                   NotificationService::AllSources());

    while (!HasFailure() && browser->tab_count() != expected_tab_count)
      ui_test_utils::RunMessageLoop();

    ASSERT_EQ(expected_tab_count, browser->tab_count());
  }

  void WaitForTabOpenOrClose(int expected_tab_count) {
    WaitForTabOpenOrCloseForBrowser(browser(), expected_tab_count);
  }

  void WaitForAutocompleteControllerDone() {
    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

    AutocompleteController* controller =
        edit_view->model()->popup_model()->autocomplete_controller();
    ASSERT_TRUE(controller);

    if (controller->done())
      return;

    NotificationRegistrar registrar;
    registrar.Add(this,
                  NotificationType::AUTOCOMPLETE_CONTROLLER_RESULT_UPDATED,
                  Source<AutocompleteController>(controller));

    while (!HasFailure() && !controller->done())
      ui_test_utils::RunMessageLoop();

    ASSERT_TRUE(controller->done());
  }

  void SetupSearchEngine() {
    TemplateURLModel* model = browser()->profile()->GetTemplateURLModel();
    ASSERT_TRUE(model);

    if (!model->loaded()) {
      NotificationRegistrar registrar;
      registrar.Add(this, NotificationType::TEMPLATE_URL_MODEL_LOADED,
                    Source<TemplateURLModel>(model));
      model->Load();
      ui_test_utils::RunMessageLoop();
    }

    ASSERT_TRUE(model->loaded());

    TemplateURL* template_url = new TemplateURL();
    template_url->SetURL(kSearchURL, 0, 0);
    template_url->set_keyword(UTF8ToUTF16(kSearchKeyword));
    template_url->set_short_name(UTF8ToUTF16(kSearchShortName));

    model->Add(template_url);
    model->SetDefaultSearchProvider(template_url);
  }

  void SetupHistory() {
    Profile* profile = browser()->profile();
    HistoryService* history_service =
        profile->GetHistoryService(Profile::EXPLICIT_ACCESS);
    ASSERT_TRUE(history_service);

    if (!history_service->BackendLoaded()) {
      NotificationRegistrar registrar;
      registrar.Add(this, NotificationType::HISTORY_LOADED,
                    Source<Profile>(profile));
      ui_test_utils::RunMessageLoop();
    }

    BookmarkModel* bookmark_model = profile->GetBookmarkModel();
    ASSERT_TRUE(bookmark_model);

    if (!bookmark_model->IsLoaded()) {
      NotificationRegistrar registrar;
      registrar.Add(this, NotificationType::BOOKMARK_MODEL_LOADED,
                    Source<Profile>(profile));
      ui_test_utils::RunMessageLoop();
    }

    // Add enough history pages containing |kSearchText| to trigger open history
    // page url in autocomplete result.
    for (size_t i = 0; i < arraysize(kHistoryEntries); i++) {
      const TestHistoryEntry& cur = kHistoryEntries[i];
      GURL url(cur.url);
      // Add everything in order of time. We don't want to have a time that
      // is "right now" or it will nondeterministically appear in the results.
      Time t = Time::Now() - TimeDelta::FromHours(i + 1);
      history_service->AddPageWithDetails(url, UTF8ToUTF16(cur.title),
                                          cur.visit_count,
                                          cur.typed_count, t, false,
                                          history::SOURCE_BROWSED);
      history_service->SetPageContents(url, UTF8ToUTF16(cur.body));
      if (cur.starred) {
        bookmark_model->SetURLStarred(url, string16(), true);
      }
    }
  }

  void SetupHostResolver() {
    for (size_t i = 0; i < arraysize(kBlockedHostnames); ++i)
      host_resolver()->AddSimulatedFailure(kBlockedHostnames[i]);
  }

  void SetupComponents() {
    ASSERT_NO_FATAL_FAILURE(SetupHostResolver());
    ASSERT_NO_FATAL_FAILURE(SetupSearchEngine());
    ASSERT_NO_FATAL_FAILURE(SetupHistory());
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::TAB_PARENTED:
      case NotificationType::TAB_CLOSED:
      case NotificationType::TEMPLATE_URL_MODEL_LOADED:
      case NotificationType::AUTOCOMPLETE_CONTROLLER_RESULT_UPDATED:
      case NotificationType::HISTORY_LOADED:
      case NotificationType::BOOKMARK_MODEL_LOADED:
        break;
      default:
        FAIL() << "Unexpected notification type";
    }
    MessageLoopForUI::current()->Quit();
  }
};

// Test if ctrl-* accelerators are workable in omnibox.
// See http://crbug.com/19193: omnibox blocks ctrl-* commands
//
// Flaky on interactive tests (dbg), http://crbug.com/69433
IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, FLAKY_BrowserAccelerators) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  browser()->FocusLocationBar();
  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

  int tab_count = browser()->tab_count();

  // Create a new Tab.
  browser()->NewTab();
  ASSERT_NO_FATAL_FAILURE(WaitForTabOpenOrClose(tab_count + 1));

  // Select the first Tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_1, true, false, false));
  ASSERT_EQ(0, browser()->selected_index());

  browser()->FocusLocationBar();

  // Select the second Tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_2, true, false, false));
  ASSERT_EQ(1, browser()->selected_index());

  browser()->FocusLocationBar();

  // Try ctrl-w to close a Tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_W, true, false, false));
  ASSERT_NO_FATAL_FAILURE(WaitForTabOpenOrClose(tab_count));

  // Try ctrl-l to focus location bar.
  edit_view->SetUserText(L"Hello world");
  EXPECT_FALSE(edit_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_L, true, false, false));
  EXPECT_TRUE(edit_view->IsSelectAll());

  // Try editing the location bar text.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RIGHT, false, false, false));
  EXPECT_FALSE(edit_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_S, false, false, false));
  EXPECT_EQ(L"Hello worlds", edit_view->GetText());

  // Try ctrl-x to cut text.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, true, true, false));
  EXPECT_FALSE(edit_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_X, true, false, false));
  EXPECT_EQ(L"Hello ", edit_view->GetText());

#if !defined(OS_CHROMEOS)
  // Try alt-f4 to close the browser.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_F4, false, false, true, false,
      NotificationType::BROWSER_CLOSED, Source<Browser>(browser())));
#endif
}

// Flakily fails and times out on Win only.  http://crbug.com/69941
#if defined(OS_WIN)
#define MAYBE_PopupAccelerators DISABLED_PopupAccelerators
#else
#define MAYBE_PopupAccelerators PopupAccelerators
#endif

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, MAYBE_PopupAccelerators) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Create a popup.
  Browser* popup = CreateBrowserForPopup(browser()->profile());
  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditViewForBrowser(popup, &edit_view));
  popup->FocusLocationBar();
  EXPECT_TRUE(edit_view->IsSelectAll());

  // Try ctrl-w to close the popup.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      popup, ui::VKEY_W, true, false, false, false,
      NotificationType::BROWSER_CLOSED, Source<Browser>(popup)));

  // Create another popup.
  popup = CreateBrowserForPopup(browser()->profile());
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditViewForBrowser(popup, &edit_view));

  // Set the edit text to "Hello world".
  edit_view->SetUserText(L"Hello world");
  EXPECT_FALSE(edit_view->IsSelectAll());
  popup->FocusLocationBar();
  EXPECT_TRUE(edit_view->IsSelectAll());

  // Try editing the location bar text -- should be disallowed.
  ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(popup, ui::VKEY_RIGHT, false,
                                            false, false));
  EXPECT_FALSE(edit_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(popup, ui::VKEY_S, false, false,
                                            false));
  EXPECT_EQ(L"Hello world", edit_view->GetText());

  // Try ctrl-x to cut text -- should be disallowed.
  ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(popup, ui::VKEY_LEFT, true, true,
                                            false));
  EXPECT_FALSE(edit_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(popup, ui::VKEY_X, true, false,
                                            false));
  EXPECT_EQ(L"Hello world", edit_view->GetText());

#if !defined(OS_CHROMEOS)
  // Try alt-f4 to close the popup.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      popup, ui::VKEY_F4, false, false, true, false,
      NotificationType::BROWSER_CLOSED, Source<Browser>(popup)));
#endif
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, BackspaceInKeywordMode) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

  // Trigger keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
  ASSERT_TRUE(edit_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, WideToUTF8(edit_view->model()->keyword()));

  // Trigger keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, false, false, false));
  ASSERT_FALSE(edit_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, WideToUTF8(edit_view->model()->keyword()));

  // Backspace without search text should bring back keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
  ASSERT_TRUE(edit_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, WideToUTF8(edit_view->model()->keyword()));

  // Trigger keyword mode again.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, false, false, false));
  ASSERT_FALSE(edit_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, WideToUTF8(edit_view->model()->keyword()));

  // Input something as search text.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

  // Should stay in keyword mode while deleting search text by pressing
  // backspace.
  for (size_t i = 0; i < arraysize(kSearchText) - 1; ++i) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(kSearchKeyword, WideToUTF8(edit_view->model()->keyword()));
  }

  // Input something as search text.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

  // Move cursor to the beginning of the search text.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_HOME, false, false, false));
  // Backspace at the beginning of the search text shall turn off
  // the keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
  ASSERT_FALSE(edit_view->model()->is_keyword_hint());
  ASSERT_EQ(std::string(), WideToUTF8(edit_view->model()->keyword()));
  ASSERT_EQ(std::string(kSearchKeyword) + kSearchText,
            WideToUTF8(edit_view->GetText()));
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, Escape) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIHistoryURL));
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

  std::wstring old_text = edit_view->GetText();
  EXPECT_FALSE(old_text.empty());
  EXPECT_TRUE(edit_view->IsSelectAll());

  // Delete all text in omnibox.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
  EXPECT_TRUE(edit_view->GetText().empty());

  // Escape shall revert the text in omnibox.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_ESCAPE, false, false, false));
  EXPECT_EQ(old_text, edit_view->GetText());
  EXPECT_TRUE(edit_view->IsSelectAll());
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, DesiredTLD) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
  AutocompletePopupModel* popup_model = edit_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  // Test ctrl-Enter.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kDesiredTLDKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());
  // ctrl-Enter triggers desired_tld feature, thus www.bar.com shall be opened.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RETURN, true, false, false));

  GURL url = browser()->GetSelectedTabContents()->GetURL();
  EXPECT_STREQ(kDesiredTLDHostname, url.host().c_str());
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, AltEnter) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

  edit_view->SetUserText(ASCIIToWide(chrome::kChromeUIHistoryURL));
  int tab_count = browser()->tab_count();
  // alt-Enter opens a new tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RETURN, false, false, true));
  ASSERT_NO_FATAL_FAILURE(WaitForTabOpenOrClose(tab_count + 1));
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, EnterToSearch) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_NO_FATAL_FAILURE(SetupHostResolver());
  ASSERT_NO_FATAL_FAILURE(SetupSearchEngine());
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
  AutocompletePopupModel* popup_model = edit_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  // Test Enter to search.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());

  // Check if the default match result is Search Primary Provider.
  ASSERT_EQ(AutocompleteMatch::SEARCH_WHAT_YOU_TYPED,
            popup_model->result().default_match()->type);

  // Open the default match.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RETURN, false, false, false));
  GURL url = browser()->GetSelectedTabContents()->GetURL();
  EXPECT_STREQ(kSearchTextURL, url.spec().c_str());

  // Test that entering a single character then Enter performs a search.
  browser()->FocusLocationBar();
  EXPECT_TRUE(edit_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchSingleCharKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());
  EXPECT_EQ(kSearchSingleChar, WideToUTF8(edit_view->GetText()));

  // Check if the default match result is Search Primary Provider.
  ASSERT_EQ(AutocompleteMatch::SEARCH_WHAT_YOU_TYPED,
            popup_model->result().default_match()->type);

  // Open the default match.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RETURN, false, false, false));
  url = browser()->GetSelectedTabContents()->GetURL();
  EXPECT_STREQ(kSearchSingleCharURL, url.spec().c_str());
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, EscapeToDefaultMatch) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
  AutocompletePopupModel* popup_model = edit_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  // Input something to trigger inline autocomplete.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());

  std::wstring old_text = edit_view->GetText();

  // Make sure inline autocomplete is triggerred.
  EXPECT_GT(old_text.length(), arraysize(kSearchText) - 1);

  size_t old_selected_line = popup_model->selected_line();
  EXPECT_EQ(0U, old_selected_line);

  // Move to another line with different text.
  size_t size = popup_model->result().size();
  while (popup_model->selected_line() < size - 1) {
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DOWN, false, false, false));
    ASSERT_NE(old_selected_line, popup_model->selected_line());
    if (old_text != edit_view->GetText())
      break;
  }

  EXPECT_NE(old_text, edit_view->GetText());

  // Escape shall revert back to the default match item.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_ESCAPE, false, false, false));
  EXPECT_EQ(old_text, edit_view->GetText());
  EXPECT_EQ(old_selected_line, popup_model->selected_line());
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, BasicTextOperations) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutBlankURL));
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

  std::wstring old_text = edit_view->GetText();
  EXPECT_EQ(UTF8ToWide(chrome::kAboutBlankURL), old_text);
  EXPECT_TRUE(edit_view->IsSelectAll());

  std::wstring::size_type start, end;
  edit_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(0U, start);
  EXPECT_EQ(old_text.size(), end);

  // Move the cursor to the end.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_END, false, false, false));
  EXPECT_FALSE(edit_view->IsSelectAll());

  // Make sure the cursor is placed correctly.
  edit_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(old_text.size(), start);
  EXPECT_EQ(old_text.size(), end);

  // Insert one character at the end. Make sure we won't insert anything after
  // the special ZWS mark used in gtk implementation.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_A, false, false, false));
  EXPECT_EQ(old_text + L"a", edit_view->GetText());

  // Delete one character from the end. Make sure we won't delete the special
  // ZWS mark used in gtk implementation.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
  EXPECT_EQ(old_text, edit_view->GetText());

  edit_view->SelectAll(true);
  EXPECT_TRUE(edit_view->IsSelectAll());
  edit_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(0U, start);
  EXPECT_EQ(old_text.size(), end);

  // Delete the content
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DELETE, false, false, false));
  EXPECT_TRUE(edit_view->IsSelectAll());
  edit_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(0U, start);
  EXPECT_EQ(0U, end);
  EXPECT_TRUE(edit_view->GetText().empty());

  // Check if RevertAll() can set text and cursor correctly.
  edit_view->RevertAll();
  EXPECT_FALSE(edit_view->IsSelectAll());
  EXPECT_EQ(old_text, edit_view->GetText());
  edit_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(old_text.size(), start);
  EXPECT_EQ(old_text.size(), end);
}

#if defined(OS_LINUX)
IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, UndoRedoLinux) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutBlankURL));
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

  std::wstring old_text = edit_view->GetText();
  EXPECT_EQ(UTF8ToWide(chrome::kAboutBlankURL), old_text);
  EXPECT_TRUE(edit_view->IsSelectAll());

  // Undo should clear the omnibox.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, true, false, false));
  EXPECT_TRUE(edit_view->GetText().empty());

  // Nothing should happen if undo again.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, true, false, false));
  EXPECT_TRUE(edit_view->GetText().empty());

  // Redo should restore the original text.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, true, true, false));
  EXPECT_EQ(old_text, edit_view->GetText());

  // Looks like the undo manager doesn't support restoring selection.
  EXPECT_FALSE(edit_view->IsSelectAll());

  // The cursor should be at the end.
  std::wstring::size_type start, end;
  edit_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(old_text.size(), start);
  EXPECT_EQ(old_text.size(), end);

  // Delete two characters.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
  EXPECT_EQ(old_text.substr(0, old_text.size() - 2), edit_view->GetText());

  // Undo delete.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, true, false, false));
  EXPECT_EQ(old_text, edit_view->GetText());

  // Redo delete.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, true, true, false));
  EXPECT_EQ(old_text.substr(0, old_text.size() - 2), edit_view->GetText());

  // Delete everything.
  edit_view->SelectAll(true);
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
  EXPECT_TRUE(edit_view->GetText().empty());

  // Undo delete everything.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, true, false, false));
  EXPECT_EQ(old_text.substr(0, old_text.size() - 2), edit_view->GetText());

  // Undo delete two characters.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, true, false, false));
  EXPECT_EQ(old_text, edit_view->GetText());

  // Undo again.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_Z, true, false, false));
  EXPECT_TRUE(edit_view->GetText().empty());
}

// See http://crbug.com/63860
IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, PrimarySelection) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  browser()->FocusLocationBar();
  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
  edit_view->SetUserText(L"Hello world");
  EXPECT_FALSE(edit_view->IsSelectAll());

  // Move the cursor to the end.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_END, false, false, false));

  // Select all text by pressing Shift+Home
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_HOME, false, true, false));
  EXPECT_TRUE(edit_view->IsSelectAll());

  // The selected content should be saved to the PRIMARY clipboard.
  EXPECT_EQ("Hello world", GetPrimarySelectionText());

  // Move the cursor to the end.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_END, false, false, false));
  EXPECT_FALSE(edit_view->IsSelectAll());

  // The content in the PRIMARY clipboard should not be cleared.
  EXPECT_EQ("Hello world", GetPrimarySelectionText());
}

// See http://crosbug.com/10306
IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest,
                       BackspaceDeleteHalfWidthKatakana) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  browser()->FocusLocationBar();
  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
  // Insert text: ﾀﾞ
  edit_view->SetUserText(UTF8ToWide("\357\276\200\357\276\236"));

  // Move the cursor to the end.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_END, false, false, false));

  // Backspace should delete one character.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
  EXPECT_EQ(UTF8ToWide("\357\276\200"), edit_view->GetText());
}

// http://crbug.com/12316
IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, PasteReplacingAll) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
  AutocompletePopupModel* popup_model = edit_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  SetClipboardText(kSearchText);

  // Paste text.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_V, true, false, false));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());

  // Inline autocomplete shouldn't be triggered.
  ASSERT_EQ(L"abc", edit_view->GetText());
}
#endif
