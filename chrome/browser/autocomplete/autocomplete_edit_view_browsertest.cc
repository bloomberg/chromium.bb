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
#include "views/events/event.h"

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#endif

#if defined(TOOLKIT_VIEWS)
#include "views/controls/textfield/native_textfield_views.h"
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

const char kInlineAutocompleteText[] = "def";
const wchar_t kInlineAutocompleteTextKeys[] = {
  ui::VKEY_D, ui::VKEY_E, ui::VKEY_F, 0
};

// Hostnames that shall be blocked by host resolver.
const char *kBlockedHostnames[] = {
  "foo",
  "*.foo.com",
  "bar",
  "*.bar.com",
  "abc",
  "*.abc.com",
  "def",
  "*.def.com",
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
  {"http://www.bar.com/1", "Page 1", kSearchText, 10, 10, false },
  {"http://www.bar.com/2", "Page 2", kSearchText, 9, 9, false },
  {"http://www.bar.com/3", "Page 3", kSearchText, 8, 8, false },
  {"http://www.bar.com/4", "Page 4", kSearchText, 7, 7, false },
  {"http://www.bar.com/5", "Page 5", kSearchText, 6, 6, false },
  {"http://www.bar.com/6", "Page 6", kSearchText, 5, 5, false },
  {"http://www.bar.com/7", "Page 7", kSearchText, 4, 4, false },
  {"http://www.bar.com/8", "Page 8", kSearchText, 3, 3, false },
  {"http://www.bar.com/9", "Page 9", kSearchText, 2, 2, false },

  // To trigger inline autocomplete.
  {"http://www.def.com", "Page def", kSearchText, 10000, 10000, true },
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

  virtual void SetUpOnMainThread() {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    ASSERT_NO_FATAL_FAILURE(SetupComponents());
    browser()->FocusLocationBar();
#if defined(TOOLKIT_VIEWS)
    if (views::NativeTextfieldViews::IsTextfieldViewsEnabled())
      return;
#endif
    ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                             VIEW_ID_LOCATION_BAR));
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
                  NotificationType::AUTOCOMPLETE_CONTROLLER_RESULT_READY,
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
    // Remove built-in template urls, like google.com, bing.com etc., as they
    // may appear as autocomplete suggests and interfere with our tests.
    model->SetDefaultSearchProvider(NULL);
    TemplateURLModel::TemplateURLVector builtins = model->GetTemplateURLs();
    for (TemplateURLModel::TemplateURLVector::const_iterator
         i = builtins.begin(); i != builtins.end(); ++i)
      model->Remove(*i);

    TemplateURL* template_url = new TemplateURL();
    template_url->SetURL(kSearchURL, 0, 0);
    template_url->set_keyword(UTF8ToUTF16(kSearchKeyword));
    template_url->set_short_name(UTF8ToUTF16(kSearchShortName));

    model->Add(template_url);
    model->SetDefaultSearchProvider(template_url);
  }

  void AddHistoryEntry(const TestHistoryEntry& entry, const Time& time) {
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

    GURL url(entry.url);
    // Add everything in order of time. We don't want to have a time that
    // is "right now" or it will nondeterministically appear in the results.
    history_service->AddPageWithDetails(url, UTF8ToUTF16(entry.title),
                                        entry.visit_count,
                                        entry.typed_count, time, false,
                                        history::SOURCE_BROWSED);
    history_service->SetPageContents(url, UTF8ToUTF16(entry.body));
    if (entry.starred)
      bookmark_model->SetURLStarred(url, string16(), true);
  }

  void SetupHistory() {
    // Add enough history pages containing |kSearchText| to trigger
    // open history page url in autocomplete result.
    for (size_t i = 0; i < arraysize(kHistoryEntries); i++) {
      // Add everything in order of time. We don't want to have a time that
      // is "right now" or it will nondeterministically appear in the results.
      Time t = Time::Now() - TimeDelta::FromHours(i + 1);
      ASSERT_NO_FATAL_FAILURE(AddHistoryEntry(kHistoryEntries[i], t));
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
      case NotificationType::AUTOCOMPLETE_CONTROLLER_RESULT_READY:
      case NotificationType::HISTORY_LOADED:
      case NotificationType::BOOKMARK_MODEL_LOADED:
        break;
      default:
        FAIL() << "Unexpected notification type";
    }
    MessageLoopForUI::current()->Quit();
  }

  void BrowserAcceleratorsTest() {
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
    edit_view->SetUserText(ASCIIToUTF16("Hello world"));
    EXPECT_FALSE(edit_view->IsSelectAll());
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_L, true, false, false));
    EXPECT_TRUE(edit_view->IsSelectAll());

    // Try editing the location bar text.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RIGHT, false, false, false));
    EXPECT_FALSE(edit_view->IsSelectAll());
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_S, false, false, false));
    EXPECT_EQ(ASCIIToUTF16("Hello worlds"), edit_view->GetText());

    // Try ctrl-x to cut text.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, true, true, false));
    EXPECT_FALSE(edit_view->IsSelectAll());
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_X, true, false, false));
    EXPECT_EQ(ASCIIToUTF16("Hello "), edit_view->GetText());

#if !defined(OS_CHROMEOS)
    // Try alt-f4 to close the browser.
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_F4, false, false, true, false,
        NotificationType::BROWSER_CLOSED, Source<Browser>(browser())));
#endif
  }

  void PopupAcceleratorsTest() {
    // Create a popup.
    Browser* popup = CreateBrowserForPopup(browser()->profile());
    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(
        GetAutocompleteEditViewForBrowser(popup, &edit_view));
    popup->FocusLocationBar();
    EXPECT_TRUE(edit_view->IsSelectAll());

    // Try ctrl-w to close the popup.
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        popup, ui::VKEY_W, true, false, false, false,
        NotificationType::BROWSER_CLOSED, Source<Browser>(popup)));

    // Create another popup.
    popup = CreateBrowserForPopup(browser()->profile());
    ASSERT_NO_FATAL_FAILURE(
        GetAutocompleteEditViewForBrowser(popup, &edit_view));

    // Set the edit text to "Hello world".
    edit_view->SetUserText(ASCIIToUTF16("Hello world"));
    EXPECT_FALSE(edit_view->IsSelectAll());
    popup->FocusLocationBar();
    EXPECT_TRUE(edit_view->IsSelectAll());

    // Try editing the location bar text -- should be disallowed.
    ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(popup, ui::VKEY_RIGHT, false,
                                              false, false));
    EXPECT_FALSE(edit_view->IsSelectAll());
    ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(popup, ui::VKEY_S, false, false,
                                              false));
    EXPECT_EQ(ASCIIToUTF16("Hello world"), edit_view->GetText());

    // Try ctrl-x to cut text -- should be disallowed.
    ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(popup, ui::VKEY_LEFT, true, true,
                                              false));
    EXPECT_FALSE(edit_view->IsSelectAll());
    ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(popup, ui::VKEY_X, true, false,
                                              false));
    EXPECT_EQ(ASCIIToUTF16("Hello world"), edit_view->GetText());

#if !defined(OS_CHROMEOS)
    // Try alt-f4 to close the popup.
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        popup, ui::VKEY_F4, false, false, true, false,
        NotificationType::BROWSER_CLOSED, Source<Browser>(popup)));
#endif
  }

  void BackspaceInKeywordModeTest() {
    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

    // Trigger keyword hint mode.
    ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
    ASSERT_TRUE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(edit_view->model()->keyword()));

    // Trigger keyword mode.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, false, false, false));
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(edit_view->model()->keyword()));

    // Backspace without search text should bring back keyword hint mode.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
    ASSERT_TRUE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(edit_view->model()->keyword()));

    // Trigger keyword mode again.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, false, false, false));
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(edit_view->model()->keyword()));

    // Input something as search text.
    ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

    // Should stay in keyword mode while deleting search text by pressing
    // backspace.
    for (size_t i = 0; i < arraysize(kSearchText) - 1; ++i) {
      ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
      ASSERT_FALSE(edit_view->model()->is_keyword_hint());
      ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(edit_view->model()->keyword()));
    }

    // Input something as search text.
    ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

    // Move cursor to the beginning of the search text.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_HOME, false, false, false));
    // Backspace at the beginning of the search text shall turn off
    // the keyword mode.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(string16(), edit_view->model()->keyword());
    ASSERT_EQ(std::string(kSearchKeyword) + kSearchText,
              UTF16ToUTF8(edit_view->GetText()));
  }

  void EscapeTest() {
    ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIHistoryURL));
    browser()->FocusLocationBar();

    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

    string16 old_text = edit_view->GetText();
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

  void DesiredTLDTest() {
    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
    AutocompletePopupModel* popup_model = edit_view->model()->popup_model();
    ASSERT_TRUE(popup_model);

    // Test ctrl-Enter.
    ASSERT_NO_FATAL_FAILURE(SendKeySequence(kDesiredTLDKeys));
    ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
    ASSERT_TRUE(popup_model->IsOpen());
    // ctrl-Enter triggers desired_tld feature, thus www.bar.com shall be
    // opened.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RETURN, true, false, false));

    GURL url = browser()->GetSelectedTabContents()->GetURL();
    EXPECT_STREQ(kDesiredTLDHostname, url.host().c_str());
  }

  void AltEnterTest() {
    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

    edit_view->SetUserText(ASCIIToUTF16(chrome::kChromeUIHistoryURL));
    int tab_count = browser()->tab_count();
    // alt-Enter opens a new tab.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RETURN, false, false, true));
    ASSERT_NO_FATAL_FAILURE(WaitForTabOpenOrClose(tab_count + 1));
  }

  void EnterToSearchTest() {
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
    EXPECT_EQ(kSearchSingleChar, UTF16ToUTF8(edit_view->GetText()));

    // Check if the default match result is Search Primary Provider.
    ASSERT_EQ(AutocompleteMatch::SEARCH_WHAT_YOU_TYPED,
              popup_model->result().default_match()->type);

    // Open the default match.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RETURN, false, false, false));
    url = browser()->GetSelectedTabContents()->GetURL();
    EXPECT_STREQ(kSearchSingleCharURL, url.spec().c_str());
  }

  void EscapeToDefaultMatchTest() {
    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
    AutocompletePopupModel* popup_model = edit_view->model()->popup_model();
    ASSERT_TRUE(popup_model);

    // Input something to trigger inline autocomplete.
    ASSERT_NO_FATAL_FAILURE(SendKeySequence(kInlineAutocompleteTextKeys));
    ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
    ASSERT_TRUE(popup_model->IsOpen());

    string16 old_text = edit_view->GetText();

    // Make sure inline autocomplete is triggerred.
    EXPECT_GT(old_text.length(), arraysize(kInlineAutocompleteText) - 1);

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

  void BasicTextOperationsTest() {
    ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutBlankURL));
    browser()->FocusLocationBar();

    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

    string16 old_text = edit_view->GetText();
    EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL), old_text);
    EXPECT_TRUE(edit_view->IsSelectAll());

    string16::size_type start, end;
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

    // Insert one character at the end. Make sure we won't insert
    // anything after the special ZWS mark used in gtk implementation.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_A, false, false, false));
    EXPECT_EQ(old_text + char16('a'), edit_view->GetText());

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

  void AcceptKeywordBySpaceTest() {
    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

    string16 text = UTF8ToUTF16(kSearchKeyword);

    // Trigger keyword hint mode.
    ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
    ASSERT_TRUE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(text, edit_view->model()->keyword());
    ASSERT_EQ(text, edit_view->GetText());

    // Trigger keyword mode by space.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, false, false, false));
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(text, edit_view->model()->keyword());
    ASSERT_TRUE(edit_view->GetText().empty());

    // Revert to keyword hint mode.
    edit_view->model()->ClearKeyword(string16());
    ASSERT_TRUE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(text, edit_view->model()->keyword());
    ASSERT_EQ(text, edit_view->GetText());

    // Keyword should also be accepted by typing an ideographic space.
    edit_view->OnBeforePossibleChange();
    edit_view->SetWindowTextAndCaretPos(text + WideToUTF16(L"\x3000"),
                                        text.length() + 1);
    edit_view->OnAfterPossibleChange();
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(text, edit_view->model()->keyword());
    ASSERT_TRUE(edit_view->GetText().empty());

    // Revert to keyword hint mode.
    edit_view->model()->ClearKeyword(string16());
    ASSERT_TRUE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(text, edit_view->model()->keyword());
    ASSERT_EQ(text, edit_view->GetText());

    // Keyword shouldn't be accepted by pasting.
    // Simulate pasting a whitespace to the end of content.
    edit_view->OnBeforePossibleChange();
    edit_view->model()->on_paste();
    edit_view->SetWindowTextAndCaretPos(text + char16(' '), text.length() + 1);
    edit_view->OnAfterPossibleChange();
    // Should be still in keyword hint mode.
    ASSERT_TRUE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(text, edit_view->model()->keyword());
    ASSERT_EQ(text + char16(' '), edit_view->GetText());

    // Keyword shouldn't be accepted by pressing space with a trailing
    // whitespace.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, false, false, false));
    ASSERT_TRUE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(text, edit_view->model()->keyword());
    ASSERT_EQ(text + ASCIIToUTF16("  "), edit_view->GetText());

    // Keyword shouldn't be accepted by deleting the trailing space.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
    ASSERT_TRUE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(text, edit_view->model()->keyword());
    ASSERT_EQ(text + char16(' '), edit_view->GetText());

    // Keyword shouldn't be accepted by pressing space in the middle
    // of content.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_LEFT, false, false, false));
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, false, false, false));
    ASSERT_TRUE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(text, edit_view->model()->keyword());
    ASSERT_EQ(text + ASCIIToUTF16("  "), edit_view->GetText());

    // Keyword shouldn't be accepted by pasting "foo bar".
    edit_view->SetUserText(string16());
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_TRUE(edit_view->model()->keyword().empty());

    edit_view->OnBeforePossibleChange();
    edit_view->model()->on_paste();
    edit_view->SetWindowTextAndCaretPos(text + ASCIIToUTF16(" bar"),
                                        text.length() + 4);
    edit_view->OnAfterPossibleChange();
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_TRUE(edit_view->model()->keyword().empty());
    ASSERT_EQ(text + ASCIIToUTF16(" bar"), edit_view->GetText());

    // Keyword could be accepted by pressing space with a selected range at the
    // end of text.
    edit_view->OnBeforePossibleChange();
    edit_view->OnInlineAutocompleteTextMaybeChanged(
        text + ASCIIToUTF16("  "), text.length());
    edit_view->OnAfterPossibleChange();
    ASSERT_TRUE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(text, edit_view->model()->keyword());
    ASSERT_EQ(text + ASCIIToUTF16("  "), edit_view->GetText());

    string16::size_type start, end;
    edit_view->GetSelectionBounds(&start, &end);
    ASSERT_NE(start, end);
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, false, false, false));
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(text, edit_view->model()->keyword());
    ASSERT_EQ(string16(), edit_view->GetText());

    edit_view->SetUserText(string16());

    // Space should accept keyword even when inline autocomplete is available.
    const TestHistoryEntry kHistoryFoobar = {
      "http://www.foobar.com", "Page foobar", kSearchText, 10000, 10000, true
    };

    // Add a history entry to trigger inline autocomplete when typing "foo".
    ASSERT_NO_FATAL_FAILURE(
        AddHistoryEntry(kHistoryFoobar, Time::Now() - TimeDelta::FromHours(1)));

    // Type "foo" to trigger inline autocomplete.
    ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
    ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
    ASSERT_TRUE(edit_view->model()->popup_model()->IsOpen());
    ASSERT_NE(text, edit_view->GetText());

    // Keyword hint shouldn't be visible.
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_TRUE(edit_view->model()->keyword().empty());

    // Trigger keyword mode by space.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_SPACE, false, false, false));
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(text, edit_view->model()->keyword());
    ASSERT_TRUE(edit_view->GetText().empty());
  }

  void NonSubstitutingKeywordTest() {
    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
    AutocompletePopupModel* popup_model = edit_view->model()->popup_model();
    ASSERT_TRUE(popup_model);

    TemplateURLModel* template_url_model =
        browser()->profile()->GetTemplateURLModel();

    // Add a non-default substituting keyword.
    TemplateURL* template_url = new TemplateURL();
    template_url->SetURL("http://abc.com/{searchTerms}", 0, 0);
    template_url->set_keyword(UTF8ToUTF16(kSearchText));
    template_url->set_short_name(UTF8ToUTF16("Search abc"));
    template_url_model->Add(template_url);

    edit_view->SetUserText(string16());

    // Non-default substituting keyword shouldn't be matched by default.
    ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
    ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
    ASSERT_TRUE(popup_model->IsOpen());

    // Check if the default match result is Search Primary Provider.
    ASSERT_EQ(AutocompleteMatch::SEARCH_WHAT_YOU_TYPED,
              popup_model->result().default_match()->type);
    ASSERT_EQ(kSearchTextURL,
              popup_model->result().default_match()->destination_url.spec());

    edit_view->SetUserText(string16());
    ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
    ASSERT_FALSE(popup_model->IsOpen());

    // Try a non-substituting keyword.
    template_url_model->Remove(template_url);
    template_url = new TemplateURL();
    template_url->SetURL("http://abc.com/", 0, 0);
    template_url->set_keyword(UTF8ToUTF16(kSearchText));
    template_url->set_short_name(UTF8ToUTF16("abc"));
    template_url_model->Add(template_url);

    // We always allow exact matches for non-substituting keywords.
    ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
    ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
    ASSERT_TRUE(popup_model->IsOpen());
    ASSERT_EQ(AutocompleteMatch::HISTORY_KEYWORD,
              popup_model->result().default_match()->type);
    ASSERT_EQ("http://abc.com/",
              popup_model->result().default_match()->destination_url.spec());
  }

  void DeleteItemTest() {
    // Disable the search provider, to make sure the popup contains only history
    // items.
    TemplateURLModel* model = browser()->profile()->GetTemplateURLModel();
    model->SetDefaultSearchProvider(NULL);

    ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutBlankURL));
    browser()->FocusLocationBar();

    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

    AutocompletePopupModel* popup_model = edit_view->model()->popup_model();
    ASSERT_TRUE(popup_model);

    string16 old_text = edit_view->GetText();

    // Input something that can match history items.
    edit_view->SetUserText(ASCIIToUTF16("bar"));
    ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
    ASSERT_TRUE(popup_model->IsOpen());

    // Delete the inline autocomplete part.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_DELETE, false, false, false));
    ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
    ASSERT_TRUE(popup_model->IsOpen());
    ASSERT_GE(popup_model->result().size(), 3U);

    string16 user_text = edit_view->GetText();
    ASSERT_EQ(ASCIIToUTF16("bar"), user_text);
    edit_view->SelectAll(true);
    ASSERT_TRUE(edit_view->IsSelectAll());

    // The first item should be the default match.
    size_t default_line = popup_model->selected_line();
    std::string default_url =
        popup_model->result().match_at(default_line).destination_url.spec();

    // Move down.
    edit_view->model()->OnUpOrDownKeyPressed(1);
    ASSERT_EQ(default_line + 1, popup_model->selected_line());
    string16 selected_text =
        popup_model->result().match_at(default_line + 1).fill_into_edit;
    // Temporary text is shown.
    ASSERT_EQ(selected_text, edit_view->GetText());
    ASSERT_FALSE(edit_view->IsSelectAll());

    // Delete the item.
    popup_model->TryDeletingCurrentItem();
    ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
    // The selected line shouldn't be changed, because we have more than two
    // items.
    ASSERT_EQ(default_line + 1, popup_model->selected_line());
    // Make sure the item is really deleted.
    ASSERT_NE(selected_text,
              popup_model->result().match_at(default_line + 1).fill_into_edit);
    selected_text =
        popup_model->result().match_at(default_line + 1).fill_into_edit;
    // New temporary text is shown.
    ASSERT_EQ(selected_text, edit_view->GetText());

    // Revert to the default match.
    ASSERT_TRUE(edit_view->model()->OnEscapeKeyPressed());
    ASSERT_EQ(default_line, popup_model->selected_line());
    ASSERT_EQ(user_text, edit_view->GetText());
    ASSERT_TRUE(edit_view->IsSelectAll());

    // Move down and up to select the default match as temporary text.
    edit_view->model()->OnUpOrDownKeyPressed(1);
    ASSERT_EQ(default_line + 1, popup_model->selected_line());
    edit_view->model()->OnUpOrDownKeyPressed(-1);
    ASSERT_EQ(default_line, popup_model->selected_line());

    selected_text = popup_model->result().match_at(default_line).fill_into_edit;
    // New temporary text is shown.
    ASSERT_EQ(selected_text, edit_view->GetText());
    ASSERT_FALSE(edit_view->IsSelectAll());

    // Delete the default item.
    popup_model->TryDeletingCurrentItem();
    ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
    // The selected line shouldn't be changed, but the default item should have
    // been changed.
    ASSERT_EQ(default_line, popup_model->selected_line());
    // Make sure the item is really deleted.
    ASSERT_NE(selected_text,
              popup_model->result().match_at(default_line).fill_into_edit);
    selected_text =
        popup_model->result().match_at(default_line).fill_into_edit;
    // New temporary text is shown.
    ASSERT_EQ(selected_text, edit_view->GetText());

    // As the current selected item is the new default item, pressing Escape key
    // should revert all directly.
    ASSERT_TRUE(edit_view->model()->OnEscapeKeyPressed());
    ASSERT_EQ(old_text, edit_view->GetText());
    ASSERT_TRUE(edit_view->IsSelectAll());
  }

  void TabMoveCursorToEndTest() {
    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

    edit_view->SetUserText(ASCIIToUTF16("Hello world"));

    // Move cursor to the beginning.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_HOME, false, false, false));

    string16::size_type start, end;
    edit_view->GetSelectionBounds(&start, &end);
    EXPECT_EQ(0U, start);
    EXPECT_EQ(0U, end);

    // Pressing tab should move cursor to the end.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, false, false, false));

    edit_view->GetSelectionBounds(&start, &end);
    EXPECT_EQ(edit_view->GetText().size(), start);
    EXPECT_EQ(edit_view->GetText().size(), end);

    // The location bar should still have focus.
    ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_LOCATION_BAR));

    // Select all text.
    edit_view->SelectAll(true);
    EXPECT_TRUE(edit_view->IsSelectAll());
    edit_view->GetSelectionBounds(&start, &end);
    EXPECT_EQ(0U, start);
    EXPECT_EQ(edit_view->GetText().size(), end);

    // Pressing tab should move cursor to the end.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, false, false, false));

    edit_view->GetSelectionBounds(&start, &end);
    EXPECT_EQ(edit_view->GetText().size(), start);
    EXPECT_EQ(edit_view->GetText().size(), end);

    // The location bar should still have focus.
    ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_LOCATION_BAR));

    // Pressing tab when cursor is at the end should change focus.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, false, false, false));

    ASSERT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_LOCATION_BAR));
  }

  void PersistKeywordModeOnTabSwitch() {
    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

    // Trigger keyword hint mode.
    ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
    ASSERT_TRUE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(edit_view->model()->keyword()));

    // Trigger keyword mode.
    ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_TAB, false, false, false));
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(edit_view->model()->keyword()));

    // Input something as search text.
    ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

    // Create a new tab.
    browser()->NewTab();

    // Switch back to the first tab.
    browser()->SelectTabContentsAt(0, true);

    // Make sure we're still in keyword mode.
    ASSERT_EQ(kSearchKeyword, UTF16ToUTF8(edit_view->model()->keyword()));
  }

  void CtrlKeyPressedWithInlineAutocompleteTest() {
    AutocompleteEditView* edit_view = NULL;
    ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
    AutocompletePopupModel* popup_model = edit_view->model()->popup_model();
    ASSERT_TRUE(popup_model);

    // Input something to trigger inline autocomplete.
    ASSERT_NO_FATAL_FAILURE(SendKeySequence(kInlineAutocompleteTextKeys));
    ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
    ASSERT_TRUE(popup_model->IsOpen());

    string16 old_text = edit_view->GetText();

    // Make sure inline autocomplete is triggerred.
    EXPECT_GT(old_text.length(), arraysize(kInlineAutocompleteText) - 1);

    // Press ctrl key.
    edit_view->model()->OnControlKeyChanged(true);

    // Inline autocomplete should still be there.
    EXPECT_EQ(old_text, edit_view->GetText());
  }

};

// Test if ctrl-* accelerators are workable in omnibox.
// See http://crbug.com/19193: omnibox blocks ctrl-* commands
//
// Flaky on interactive tests (dbg), http://crbug.com/69433
IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, FLAKY_BrowserAccelerators) {
  BrowserAcceleratorsTest();
}

// Flakily fails and times out on Win only.  http://crbug.com/69941
#if defined(OS_WIN)
#define MAYBE_PopupAccelerators DISABLED_PopupAccelerators
#else
#define MAYBE_PopupAccelerators PopupAccelerators
#endif

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, MAYBE_PopupAccelerators) {
  PopupAcceleratorsTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, BackspaceInKeywordMode) {
  BackspaceInKeywordModeTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, Escape) {
  EscapeTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, DesiredTLD) {
  DesiredTLDTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, AltEnter) {
  AltEnterTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, EnterToSearch) {
  EnterToSearchTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, EscapeToDefaultMatch) {
  EscapeToDefaultMatchTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, BasicTextOperations) {
  BasicTextOperationsTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, AcceptKeywordBySpace) {
  AcceptKeywordBySpaceTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, NonSubstitutingKeywordTest) {
  NonSubstitutingKeywordTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, DeleteItem) {
  DeleteItemTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, TabMoveCursorToEnd) {
  TabMoveCursorToEndTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest,
                       PersistKeywordModeOnTabSwitch) {
  PersistKeywordModeOnTabSwitch();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest,
                       CtrlKeyPressedWithInlineAutocompleteTest) {
  CtrlKeyPressedWithInlineAutocompleteTest();
}

#if defined(OS_LINUX)
// TODO(oshima): enable these tests for views-implmentation when
// these featuers are supported.

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, UndoRedoLinux) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutBlankURL));
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

  string16 old_text = edit_view->GetText();
  EXPECT_EQ(UTF8ToUTF16(chrome::kAboutBlankURL), old_text);
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
  string16::size_type start, end;
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
  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
  edit_view->SetUserText(ASCIIToUTF16("Hello world"));
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
  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
  // Insert text: ﾀﾞ
  edit_view->SetUserText(UTF8ToUTF16("\357\276\200\357\276\236"));

  // Move the cursor to the end.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_END, false, false, false));

  // Backspace should delete one character.
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_BACK, false, false, false));
  EXPECT_EQ(UTF8ToUTF16("\357\276\200"), edit_view->GetText());
}

// http://crbug.com/12316
IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, PasteReplacingAll) {
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
  ASSERT_EQ(ASCIIToUTF16("abc"), edit_view->GetText());
}
#endif

#if defined(TOOLKIT_VIEWS)
class AutocompleteEditViewViewsTest : public AutocompleteEditViewTest {
 public:
  AutocompleteEditViewViewsTest() {
    views::NativeTextfieldViews::SetEnableTextfieldViews(true);
  }
};

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest,
                       FLAKY_BrowserAccelerators) {
  BrowserAcceleratorsTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest, MAYBE_PopupAccelerators) {
  PopupAcceleratorsTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest, BackspaceInKeywordMode) {
  BackspaceInKeywordModeTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest, Escape) {
  EscapeTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest, DesiredTLD) {
  DesiredTLDTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest, AltEnter) {
  AltEnterTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest, EnterToSearch) {
  EnterToSearchTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest, EscapeToDefaultMatch) {
  EscapeToDefaultMatchTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest, BasicTextOperations) {
  BasicTextOperationsTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest, AcceptKeywordBySpace) {
  AcceptKeywordBySpaceTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest,
                       NonSubstitutingKeywordTest) {
  NonSubstitutingKeywordTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest, DeleteItem) {
  DeleteItemTest();
}

// TODO(suzhe): This test is broken because of broken ViewID support when
// enabling AutocompleteEditViewViews.
IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest,
                       DISABLED_TabMoveCursorToEnd) {
  TabMoveCursorToEndTest();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest,
                       PersistKeywordModeOnTabSwitch) {
  PersistKeywordModeOnTabSwitch();
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewViewsTest,
                       CtrlKeyPressedWithInlineAutocompleteTest) {
  CtrlKeyPressedWithInlineAutocompleteTest();
}

#endif
