// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/file_path.h"
#include "base/keyboard_codes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"
#include "views/event.h"

using base::Time;
using base::TimeDelta;

namespace {

const char kSearchKeyword[] = "foo";
const wchar_t kSearchKeywordKeys[] = {
  base::VKEY_F, base::VKEY_O, base::VKEY_O, 0
};
const char kSearchURL[] = "http://www.foo.com/search?q={searchTerms}";
const char kSearchShortName[] = "foo";
const char kSearchText[] = "abc";
const wchar_t kSearchTextKeys[] = {
  base::VKEY_A, base::VKEY_B, base::VKEY_C, 0
};
const char kSearchTextURL[] = "http://www.foo.com/search?q=abc";
const char kSearchSingleChar[] = "z";
const wchar_t kSearchSingleCharKeys[] = { base::VKEY_Z, 0 };
const char kSearchSingleCharURL[] = "http://www.foo.com/search?q=z";

const char kHistoryPageURL[] = "chrome://history/#q=abc";

const char kDesiredTLDHostname[] = "www.bar.com";
const wchar_t kDesiredTLDKeys[] = {
  base::VKEY_B, base::VKEY_A, base::VKEY_R, 0
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

}  // namespace

class AutocompleteEditViewTest : public InProcessBrowserTest,
                                 public NotificationObserver {
 protected:
  AutocompleteEditViewTest() {
    set_show_window(true);
  }

  void GetNativeWindow(gfx::NativeWindow* native_window) {
    BrowserWindow* window = browser()->window();
    ASSERT_TRUE(window);
    *native_window = window->GetNativeHandle();
    ASSERT_TRUE(*native_window);
  }

  void GetAutocompleteEditView(AutocompleteEditView** edit_view) {
    BrowserWindow* window = browser()->window();
    ASSERT_TRUE(window);
    LocationBar* loc_bar = window->GetLocationBar();
    ASSERT_TRUE(loc_bar);
    *edit_view = loc_bar->location_entry();
    ASSERT_TRUE(*edit_view);
  }

  void SendKey(base::KeyboardCode key, bool control, bool shift, bool alt) {
    gfx::NativeWindow window = NULL;
    ASSERT_NO_FATAL_FAILURE(GetNativeWindow(&window));
    ui_controls::SendKeyPressNotifyWhenDone(window, key, control, shift, alt,
                                            false /* command */,
                                            new MessageLoop::QuitTask());
    ui_test_utils::RunMessageLoop();
  }

  void SendKeySequence(const wchar_t* keys) {
    for (; *keys; ++keys)
      ASSERT_NO_FATAL_FAILURE(SendKey(static_cast<base::KeyboardCode>(*keys),
                                      false, false, false));
  }

  void WaitForTabOpenOrClose(int expected_tab_count) {
    int tab_count = browser()->tab_count();
    if (tab_count == expected_tab_count)
      return;

    NotificationRegistrar registrar;
    registrar.Add(this,
                  (tab_count < expected_tab_count ?
                   NotificationType::TAB_PARENTED :
                   NotificationType::TAB_CLOSED),
                   NotificationService::AllSources());

    while (!HasFailure() && browser()->tab_count() != expected_tab_count)
      ui_test_utils::RunMessageLoop();

    ASSERT_EQ(expected_tab_count, browser()->tab_count());
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
    template_url->set_keyword(UTF8ToWide(kSearchKeyword));
    template_url->set_short_name(UTF8ToWide(kSearchShortName));

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
                                          cur.typed_count, t, false);
      history_service->SetPageContents(url, UTF8ToUTF16(cur.body));
      if (cur.starred) {
        bookmark_model->SetURLStarred(url, std::wstring(), true);
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

#if defined(OS_WIN)
  virtual void CleanUpOnMainThread() {
    // A hack to avoid hitting issue http://crbug.com/18372 and
    // http://crbug.com/18373
    // Weird that it only happens on Windows.
    // TODO(suzhe): Remove this hack as soon as these bugs are fixed.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            new MessageLoop::QuitTask(),
                                            2000);
    ui_test_utils::RunMessageLoop();
  }
#endif
};

// Test if ctrl-* accelerators are workable in omnibox.
// See http://crbug.com/19193: omnibox blocks ctrl-* commands
// This test is disabled. See bug 23213.
IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, BrowserAccelerators) {
  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  browser()->FocusLocationBar();
  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

  int tab_count = browser()->tab_count();

  // Create a new Tab.
  browser()->NewTab();
  ASSERT_NO_FATAL_FAILURE(WaitForTabOpenOrClose(tab_count + 1));

  // Select the first Tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_1, true, false, false));
  ASSERT_EQ(0, browser()->selected_index());

  browser()->FocusLocationBar();

  // Select the second Tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_2, true, false, false));
  ASSERT_EQ(1, browser()->selected_index());

  browser()->FocusLocationBar();

  // Close a Tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_W, true, false, false));
  ASSERT_NO_FATAL_FAILURE(WaitForTabOpenOrClose(tab_count));

  // Try ctrl-l to focus location bar.
  edit_view->SetUserText(L"Hello world");
  EXPECT_FALSE(edit_view->IsSelectAll());
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_L, true, false, false));
  EXPECT_TRUE(edit_view->IsSelectAll());
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, BackspaceInKeywordMode) {
  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

  // Trigger keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchKeywordKeys));
  ASSERT_TRUE(edit_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, WideToUTF8(edit_view->model()->keyword()));

  // Trigger keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_TAB, false, false, false));
  ASSERT_FALSE(edit_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, WideToUTF8(edit_view->model()->keyword()));

  // Backspace without search text should bring back keyword hint mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_BACK, false, false, false));
  ASSERT_TRUE(edit_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, WideToUTF8(edit_view->model()->keyword()));

  // Trigger keyword mode again.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_TAB, false, false, false));
  ASSERT_FALSE(edit_view->model()->is_keyword_hint());
  ASSERT_EQ(kSearchKeyword, WideToUTF8(edit_view->model()->keyword()));

  // Input something as search text.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

  // Should stay in keyword mode while deleting search text by pressing
  // backspace.
  for (size_t i = 0; i < arraysize(kSearchText) - 1; ++i) {
    ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_BACK, false, false, false));
    ASSERT_FALSE(edit_view->model()->is_keyword_hint());
    ASSERT_EQ(kSearchKeyword, WideToUTF8(edit_view->model()->keyword()));
  }

  // Input something as search text.
  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));

  // Move cursor to the beginning of the search text.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_HOME, false, false, false));
  // Backspace at the beginning of the search text shall turn off
  // the keyword mode.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_BACK, false, false, false));
  ASSERT_FALSE(edit_view->model()->is_keyword_hint());
  ASSERT_EQ(std::string(), WideToUTF8(edit_view->model()->keyword()));
  ASSERT_EQ(std::string(kSearchKeyword) + kSearchText,
            WideToUTF8(edit_view->GetText()));
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, Escape) {
  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIHistoryURL));
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

  std::wstring old_text = edit_view->GetText();
  EXPECT_FALSE(old_text.empty());
  EXPECT_TRUE(edit_view->IsSelectAll());

  // Delete all text in omnibox.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_BACK, false, false, false));
  EXPECT_TRUE(edit_view->GetText().empty());

  // Escape shall revert the text in omnibox.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_ESCAPE, false, false, false));
  EXPECT_EQ(old_text, edit_view->GetText());
  EXPECT_TRUE(edit_view->IsSelectAll());
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, DesiredTLD) {
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
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_RETURN, true, false, false));

  GURL url = browser()->GetSelectedTabContents()->GetURL();
  EXPECT_STREQ(kDesiredTLDHostname, url.host().c_str());
}

// This test is disabled. See bug 23213.
IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, AltEnter) {
  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));

  edit_view->SetUserText(ASCIIToWide(chrome::kChromeUIHistoryURL));
  int tab_count = browser()->tab_count();
  // alt-Enter opens a new tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_RETURN, false, false, true));
  ASSERT_NO_FATAL_FAILURE(WaitForTabOpenOrClose(tab_count + 1));
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, EnterToSearch) {
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
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_RETURN, false, false, false));
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
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_RETURN, false, false, false));
  url = browser()->GetSelectedTabContents()->GetURL();
  EXPECT_STREQ(kSearchSingleCharURL, url.spec().c_str());
}

// See http://crbug.com/20934: Omnibox keyboard behavior wrong for
// "See recent pages in history"
IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, EnterToOpenHistoryPage) {
  ASSERT_NO_FATAL_FAILURE(SetupComponents());
  browser()->FocusLocationBar();

  AutocompleteEditView* edit_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetAutocompleteEditView(&edit_view));
  AutocompletePopupModel* popup_model = edit_view->model()->popup_model();
  ASSERT_TRUE(popup_model);

  ASSERT_NO_FATAL_FAILURE(SendKeySequence(kSearchTextKeys));
  ASSERT_NO_FATAL_FAILURE(WaitForAutocompleteControllerDone());
  ASSERT_TRUE(popup_model->IsOpen());
  EXPECT_EQ(0U, popup_model->selected_line());

  // Move to the history page item.
  size_t size = popup_model->result().size();
  while (true) {
    if (popup_model->result().match_at(popup_model->selected_line()).type ==
        AutocompleteMatch::OPEN_HISTORY_PAGE)
      break;
    size_t old_selected_line = popup_model->selected_line();
    ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_DOWN, false, false, false));
    ASSERT_EQ(old_selected_line + 1, popup_model->selected_line());
    if (popup_model->selected_line() == size - 1)
      break;
  }

  // Make sure the history page item is selected.
  ASSERT_EQ(AutocompleteMatch::OPEN_HISTORY_PAGE,
            popup_model->result().match_at(popup_model->selected_line()).type);

  // Open the history page item.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_RETURN, false, false, false));
  GURL url = browser()->GetSelectedTabContents()->GetURL();
  EXPECT_STREQ(kHistoryPageURL, url.spec().c_str());
}

IN_PROC_BROWSER_TEST_F(AutocompleteEditViewTest, EscapeToDefaultMatch) {
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
    ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_DOWN, false, false, false));
    ASSERT_NE(old_selected_line, popup_model->selected_line());
    if (old_text != edit_view->GetText())
      break;
  }

  EXPECT_NE(old_text, edit_view->GetText());

  // Escape shall revert back to the default match item.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_ESCAPE, false, false, false));
  EXPECT_EQ(old_text, edit_view->GetText());
  EXPECT_EQ(old_selected_line, popup_model->selected_line());
}
