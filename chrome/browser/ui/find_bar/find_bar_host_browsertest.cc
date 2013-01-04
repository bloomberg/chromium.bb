// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_util.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/keycodes/keyboard_codes.h"

using content::NavigationController;
using content::WebContents;

const std::string kAnchorPage = "anchor.html";
const std::string kAnchor = "#chapter2";
const std::string kFramePage = "frames.html";
const std::string kFrameData = "framedata_general.html";
const std::string kUserSelectPage = "user-select.html";
const std::string kCrashPage = "crash_1341577.html";
const std::string kTooFewMatchesPage = "bug_1155639.html";
const std::string kLongTextareaPage = "large_textarea.html";
const std::string kEndState = "end_state.html";
const std::string kPrematureEnd = "premature_end.html";
const std::string kMoveIfOver = "move_if_obscuring.html";
const std::string kBitstackCrash = "crash_14491.html";
const std::string kSelectChangesOrdinal = "select_changes_ordinal.html";
const std::string kStartAfterSelection = "start_after_selection.html";
const std::string kSimple = "simple.html";
const std::string kLinkPage = "link.html";

const bool kBack = false;
const bool kFwd = true;

const bool kIgnoreCase = false;
const bool kCaseSensitive = true;

const int kMoveIterations = 30;

namespace {

void HistoryServiceQueried(int) {
  MessageLoop::current()->Quit();
}

}  // namespace

class FindInPageControllerTest : public InProcessBrowserTest {
 public:
  FindInPageControllerTest() {
    chrome::DisableFindBarAnimationsDuringTesting(true);
  }

 protected:
  bool GetFindBarWindowInfoForBrowser(
      Browser* browser, gfx::Point* position, bool* fully_visible) {
    FindBarTesting* find_bar =
        browser->GetFindBarController()->find_bar()->GetFindBarTesting();
    return find_bar->GetFindBarWindowInfo(position, fully_visible);
  }

  bool GetFindBarWindowInfo(gfx::Point* position, bool* fully_visible) {
    return GetFindBarWindowInfoForBrowser(browser(), position, fully_visible);
  }

  string16 GetFindBarTextForBrowser(Browser* browser) {
    FindBarTesting* find_bar =
        browser->GetFindBarController()->find_bar()->GetFindBarTesting();
    return find_bar->GetFindText();
  }

  string16 GetFindBarText() {
    return GetFindBarTextForBrowser(browser());
  }

  string16 GetFindBarMatchCountTextForBrowser(Browser* browser) {
    FindBarTesting* find_bar =
        browser->GetFindBarController()->find_bar()->GetFindBarTesting();
    return find_bar->GetMatchCountText();
  }

  string16 GetMatchCountText() {
    return GetFindBarMatchCountTextForBrowser(browser());
  }

  int GetFindBarWidthForBrowser(Browser* browser) {
    FindBarTesting* find_bar =
        browser->GetFindBarController()->find_bar()->GetFindBarTesting();
    return find_bar->GetWidth();
  }

  void EnsureFindBoxOpenForBrowser(Browser* browser) {
    chrome::ShowFindBar(browser);
    gfx::Point position;
    bool fully_visible = false;
    EXPECT_TRUE(GetFindBarWindowInfoForBrowser(
                    browser, &position, &fully_visible));
    EXPECT_TRUE(fully_visible);
  }

  void EnsureFindBoxOpen() {
    EnsureFindBoxOpenForBrowser(browser());
  }

  // Platform independent FindInPage that takes |const wchar_t*|
  // as an input.
  int FindInPageWchar(WebContents* web_contents,
                      const wchar_t* search_str,
                      bool forward,
                      bool case_sensitive,
                      int* ordinal) {
    return ui_test_utils::FindInPage(
        web_contents, WideToUTF16(std::wstring(search_str)),
        forward, case_sensitive, ordinal, NULL);
  }

  // Calls FindInPageWchar till the find box's x position != |start_x_position|.
  // Return |start_x_position| if the find box has not moved after iterating
  // through all matches of |search_str|.
  int FindInPageTillBoxMoves(WebContents* web_contents,
                             int start_x_position,
                             const wchar_t* search_str,
                             int expected_matches) {
    // Search for |search_str| which the Find box is obscuring.
    for (int index = 0; index < expected_matches; ++index) {
      int ordinal = 0;
      EXPECT_EQ(expected_matches, FindInPageWchar(web_contents, search_str,
                                                  kFwd, kIgnoreCase, &ordinal));

      // Check the position.
      bool fully_visible;
      gfx::Point position;
      EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
      EXPECT_TRUE(fully_visible);

      // If the Find box has moved then we are done.
      if (position.x() != start_x_position)
        return position.x();
    }
    return start_x_position;
  }

  GURL GetURL(const std::string filename) {
    return ui_test_utils::GetTestUrl(
        FilePath().AppendASCII("find_in_page"),
        FilePath().AppendASCII(filename));
  }

  void FlushHistoryService() {
    HistoryServiceFactory::GetForProfile(
        browser()->profile(), Profile::IMPLICIT_ACCESS)->
        GetNextDownloadId(base::Bind(&HistoryServiceQueried));
    content::RunMessageLoop();
  }
};

// This test loads a page with frames and starts FindInPage requests.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageFrames) {
  // First we navigate to our frames page.
  GURL url = GetURL(kFramePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Try incremental search (mimicking user typing in).
  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(18, FindInPageWchar(web_contents, L"g",
                                kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(11, FindInPageWchar(web_contents, L"go",
                                kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(4, FindInPageWchar(web_contents, L"goo",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(3, FindInPageWchar(web_contents, L"goog",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(2, FindInPageWchar(web_contents, L"googl",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"google",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(0, FindInPageWchar(web_contents, L"google!",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);

  // Negative test (no matches should be found).
  EXPECT_EQ(0, FindInPageWchar(web_contents, L"Non-existing string",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);

  // 'horse' only exists in the three right frames.
  EXPECT_EQ(3, FindInPageWchar(web_contents, L"horse",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // 'cat' only exists in the first frame.
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"cat",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Try searching again, should still come up with 1 match.
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"cat",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Try searching backwards, ignoring case, should still come up with 1 match.
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"CAT",
                               kBack, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Try case sensitive, should NOT find it.
  EXPECT_EQ(0, FindInPageWchar(web_contents, L"CAT",
                               kFwd, kCaseSensitive, &ordinal));
  EXPECT_EQ(0, ordinal);

  // Try again case sensitive, but this time with right case.
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"dog",
                               kFwd, kCaseSensitive, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Try non-Latin characters ('Hreggvidur' with 'eth' for 'd' in left frame).
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"Hreggvi\u00F0ur",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"Hreggvi\u00F0ur",
                               kFwd, kCaseSensitive, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(0, FindInPageWchar(web_contents, L"hreggvi\u00F0ur",
                               kFwd, kCaseSensitive, &ordinal));
  EXPECT_EQ(0, ordinal);
}

// Verify search for text within various forms and text areas.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageFormsTextAreas) {
  std::vector<GURL> urls;
  urls.push_back(GetURL("textintextarea.html"));
  urls.push_back(GetURL("smalltextarea.html"));
  urls.push_back(GetURL("populatedform.html"));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  for (size_t i = 0; i < urls.size(); ++i) {
    ui_test_utils::NavigateToURL(browser(), urls[i]);
    EXPECT_EQ(1, FindInPageWchar(web_contents, L"cat",
                                 kFwd, kIgnoreCase, NULL));
    EXPECT_EQ(0, FindInPageWchar(web_contents, L"bat",
                                 kFwd, kIgnoreCase, NULL));
  }
}

// Verify search for text within special URLs such as chrome:history,
// chrome://downloads, data directory
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, SearchWithinSpecialURL) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  FilePath data_dir = ui_test_utils::GetTestFilePath(FilePath(), FilePath());
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(data_dir));
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"downloads",
                               kFwd, kIgnoreCase, NULL));

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIHistoryURL));

  // The history page does an async request to the history service and then
  // updates the renderer. So we make a query as well, and by the time it comes
  // back we know the data is on its way to the renderer.
  FlushHistoryService();

  string16 query(data_dir.LossyDisplayName());
  EXPECT_EQ(1,
            ui_test_utils::FindInPage(web_contents, query,
                                      kFwd, kIgnoreCase, NULL, NULL));

  GURL download_url = ui_test_utils::GetTestUrl(
      FilePath().AppendASCII("downloads"),
      FilePath().AppendASCII("a_zip_file.zip"));
  ui_test_utils::DownloadURL(browser(), download_url);

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIDownloadsURL));
  FlushHistoryService();
  EXPECT_EQ(1,
            FindInPageWchar(web_contents,
                            ASCIIToWide(download_url.spec()).c_str(), kFwd,
                            kIgnoreCase, NULL));
}

// Verify search selection coordinates. The data file used is set-up such that
// the text occurs on the same line, and we verify their positions by verifying
// their relative positions.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageSpecialURLs) {
  std::wstring search_string(L"\u5728\u897f\u660c\u536b\u661f\u53d1");
  gfx::Rect first, second, first_reverse;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), GetURL("specialchar.html"));
  ui_test_utils::FindInPage(web_contents, WideToUTF16(search_string),
                            kFwd, kIgnoreCase, NULL, &first);
  ui_test_utils::FindInPage(web_contents, WideToUTF16(search_string),
                            kFwd, kIgnoreCase, NULL, &second);

  // We have search occurrence in the same row, so top-bottom coordinates should
  // be the same even for second search.
  ASSERT_EQ(first.y(), second.y());
  ASSERT_EQ(first.bottom(), second.bottom());
  ASSERT_LT(first.x(), second.x());
  ASSERT_LT(first.right(), second.right());

  ui_test_utils::FindInPage(
      web_contents, WideToUTF16(search_string), kBack, kIgnoreCase, NULL,
      &first_reverse);
  // We find next and we go back so find coordinates should be the same as
  // previous ones.
  ASSERT_EQ(first, first_reverse);
}

// Verifies that comments and meta data are not searchable.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       CommentsAndMetaDataNotSearchable) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), GetURL("specialchar.html"));

  std::wstring search_string =
      L"\u4e2d\u65b0\u793e\u8bb0\u8005\u5b8b\u5409\u6cb3\u6444\u4e2d\u65b0\u7f51";
  EXPECT_EQ(0, ui_test_utils::FindInPage(
      web_contents, WideToUTF16(search_string), kFwd, kIgnoreCase, NULL, NULL));
}

// Verifies that span and lists are searchable.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, SpanAndListsSearchable) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), GetURL("FindRandomTests.html"));

  std::wstring search_string = L"has light blue eyes and my father has dark";
  EXPECT_EQ(1, ui_test_utils::FindInPage(
      web_contents, WideToUTF16(search_string), kFwd, kIgnoreCase, NULL, NULL));

  search_string = L"Google\nApple\nandroid";
  EXPECT_EQ(1, ui_test_utils::FindInPage(
      web_contents, WideToUTF16(search_string), kFwd, kIgnoreCase, NULL, NULL));
}

// Find in a very large page.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, LargePage) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), GetURL("largepage.html"));

  std::wstring search_string = L"daughter of Prince";
  EXPECT_EQ(373,
            FindInPageWchar(web_contents, search_string.c_str(),
                            kFwd, kIgnoreCase, NULL));
}

// Find a very long string in a large page.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindLongString) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), GetURL("largepage.html"));

  FilePath path = ui_test_utils::GetTestFilePath(
      FilePath().AppendASCII("find_in_page"),
      FilePath().AppendASCII("LongFind.txt"));
  std::string query;
  file_util::ReadFileToString(path, &query);
  std::wstring search_string = UTF8ToWide(query);
  EXPECT_EQ(1,
            FindInPageWchar(web_contents, search_string.c_str(),
                            kFwd, kIgnoreCase, NULL));
}

// Find a big font string in a page.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, BigString) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), GetURL("BigText.html"));
  EXPECT_EQ(1,
            FindInPageWchar(web_contents, L"SomeLargeString",
                            kFwd, kIgnoreCase, NULL));
}

// Search Back and Forward on a single occurrence.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, SingleOccurrence) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), GetURL("FindRandomTests.html"));

  gfx::Rect first_rect;
  EXPECT_EQ(1,
            ui_test_utils::FindInPage(web_contents,
                                      ASCIIToUTF16("2010 Pro Bowl"), kFwd,
                                      kIgnoreCase, NULL, &first_rect));

  gfx::Rect second_rect;
  EXPECT_EQ(1,
            ui_test_utils::FindInPage(web_contents,
                                      ASCIIToUTF16("2010 Pro Bowl"), kFwd,
                                      kIgnoreCase, NULL, &second_rect));

  // Doing a fake find so we have no previous search.
  ui_test_utils::FindInPage(web_contents, ASCIIToUTF16("ghgfjgfh201232rere"),
                            kFwd, kIgnoreCase, NULL, NULL);

  ASSERT_EQ(first_rect, second_rect);

  EXPECT_EQ(1,
            ui_test_utils::FindInPage(web_contents,
                                      ASCIIToUTF16("2010 Pro Bowl"), kFwd,
                                      kIgnoreCase, NULL, &first_rect));
  EXPECT_EQ(1,
            ui_test_utils::FindInPage(web_contents,
                                      ASCIIToUTF16("2010 Pro Bowl"), kBack,
                                      kIgnoreCase, NULL, &second_rect));
  ASSERT_EQ(first_rect, second_rect);
}

// Find the whole text file page and find count should be 1.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindWholeFileContent) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  FilePath path = ui_test_utils::GetTestFilePath(
      FilePath().AppendASCII("find_in_page"),
      FilePath().AppendASCII("find_test.txt"));
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(path));

  std::string query;
  file_util::ReadFileToString(path, &query);
  std::wstring search_string = UTF8ToWide(query);
  EXPECT_EQ(1,
            FindInPageWchar(web_contents, search_string.c_str(),
                            false, false, NULL));
}

// Specifying a prototype so that we can add the WARN_UNUSED_RESULT attribute.
bool FocusedOnPage(WebContents* web_contents, std::string* result)
    WARN_UNUSED_RESULT;

bool FocusedOnPage(WebContents* web_contents, std::string* result) {
  return content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(getFocusedElement());",
      result);
}

// This tests the FindInPage end-state, in other words: what is focused when you
// close the Find box (ie. if you find within a link the link should be
// focused).
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageEndState) {
  // First we navigate to our special focus tracking page.
  GURL url = GetURL(kEndState);
  ui_test_utils::NavigateToURL(browser(), url);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NULL != web_contents);
  FindTabHelper* find_tab_helper =
      FindTabHelper::FromWebContents(web_contents);

  // Verify that nothing has focus.
  std::string result;
  ASSERT_TRUE(FocusedOnPage(web_contents, &result));
  ASSERT_STREQ("{nothing focused}", result.c_str());

  // Search for a text that exists within a link on the page.
  int ordinal = 0;
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"nk",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // End the find session, which should set focus to the link.
  find_tab_helper->StopFinding(FindBarController::kKeepSelectionOnPage);

  // Verify that the link is focused.
  ASSERT_TRUE(FocusedOnPage(web_contents, &result));
  EXPECT_STREQ("link1", result.c_str());

  // Search for a text that exists within a link on the page.
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"Google",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Move the selection to link 1, after searching.
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(selectLink1());",
      &result));

  // End the find session.
  find_tab_helper->StopFinding(FindBarController::kKeepSelectionOnPage);

  // Verify that link2 is not focused.
  ASSERT_TRUE(FocusedOnPage(web_contents, &result));
  EXPECT_STREQ("", result.c_str());
}

// This test loads a single-frame page and makes sure the ordinal returned makes
// sense as we FindNext over all the items.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageOrdinal) {
  // First we navigate to our page.
  GURL url = GetURL(kFrameData);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for 'o', which should make the first item active and return
  // '1 in 3' (1st ordinal of a total of 3 matches).
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int ordinal = 0;
  EXPECT_EQ(3, FindInPageWchar(web_contents, L"o",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(3, FindInPageWchar(web_contents, L"o",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(3, FindInPageWchar(web_contents, L"o",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
  // Go back one match.
  EXPECT_EQ(3, FindInPageWchar(web_contents, L"o",
                               kBack, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(3, FindInPageWchar(web_contents, L"o",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
  // This should wrap to the top.
  EXPECT_EQ(3, FindInPageWchar(web_contents, L"o",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  // This should go back to the end.
  EXPECT_EQ(3, FindInPageWchar(web_contents, L"o",
                               kBack, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
}

// This tests that the ordinal is correctly adjusted after a selection
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       SelectChangesOrdinal_Issue20883) {
  // First we navigate to our test content.
  GURL url = GetURL(kSelectChangesOrdinal);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for a text that exists within a link on the page.
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NULL != web_contents);
  FindTabHelper* find_tab_helper =
      FindTabHelper::FromWebContents(web_contents);

  int ordinal = 0;
  EXPECT_EQ(4, FindInPageWchar(web_contents,
                               L"google",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Move the selection to link 1, after searching.
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(selectLink1());",
      &result));

  // Do a find-next after the selection.  This should move forward
  // from there to the 3rd instance of 'google'.
  EXPECT_EQ(4, FindInPageWchar(web_contents,
                               L"google",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);

  // End the find session.
  find_tab_helper->StopFinding(FindBarController::kKeepSelectionOnPage);
}

// This tests that we start searching after selected text.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       StartSearchAfterSelection) {
  // First we navigate to our test content.
  ui_test_utils::NavigateToURL(browser(), GetURL(kStartAfterSelection));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents != NULL);
  int ordinal = 0;

  // Move the selection to the text span.
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(selectSpan());",
      &result));

  // Do a find-next after the selection. This should select the 2nd occurrence
  // of the word 'find'.
  EXPECT_EQ(4, FindInPageWchar(web_contents,
                               L"fi",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);

  // Refine the search, current active match should not change.
  EXPECT_EQ(4, FindInPageWchar(web_contents,
                               L"find",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);

  // Refine the search to 'findMe'. The first new match is before the current
  // active match, the second one is after it. This verifies that refining a
  // search doesn't reset it.
  EXPECT_EQ(2, FindInPageWchar(web_contents,
                               L"findMe",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);
}

// This test loads a page with frames and makes sure the ordinal returned makes
// sense.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageMultiFramesOrdinal) {
  // First we navigate to our page.
  GURL url = GetURL(kFramePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for 'a', which should make the first item active and return
  // '1 in 7' (1st ordinal of a total of 7 matches).
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int ordinal = 0;
  EXPECT_EQ(7,
            FindInPageWchar(web_contents, L"a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(7,
            FindInPageWchar(web_contents, L"a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(7,
            FindInPageWchar(web_contents, L"a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
  EXPECT_EQ(7,
            FindInPageWchar(web_contents, L"a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(4, ordinal);
  // Go back one, which should go back one frame.
  EXPECT_EQ(7,
            FindInPageWchar(web_contents, L"a", kBack, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
  EXPECT_EQ(7,
            FindInPageWchar(web_contents, L"a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(4, ordinal);
  EXPECT_EQ(7,
            FindInPageWchar(web_contents, L"a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(5, ordinal);
  EXPECT_EQ(7,
            FindInPageWchar(web_contents, L"a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(6, ordinal);
  EXPECT_EQ(7,
            FindInPageWchar(web_contents, L"a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(7, ordinal);
  // Now we should wrap back to frame 1.
  EXPECT_EQ(7,
            FindInPageWchar(web_contents, L"a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  // Now we should wrap back to frame last frame.
  EXPECT_EQ(7,
            FindInPageWchar(web_contents, L"a", kBack, kIgnoreCase, &ordinal));
  EXPECT_EQ(7, ordinal);
}

// We could get ordinals out of whack when restarting search in subframes.
// See http://crbug.com/5132.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPage_Issue5132) {
  // First we navigate to our page.
  GURL url = GetURL(kFramePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for 'goa' three times (6 matches on page).
  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(6, FindInPageWchar(web_contents, L"goa",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(6, FindInPageWchar(web_contents, L"goa",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(6, FindInPageWchar(web_contents, L"goa",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
  // Add space to search (should result in no matches).
  EXPECT_EQ(0, FindInPageWchar(web_contents, L"goa ",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);
  // Remove the space, should be back to '3 out of 6')
  EXPECT_EQ(6, FindInPageWchar(web_contents, L"goa",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
}

// Mac doesn't implement GetFindBarText() and GetMatchCountText(), which are
// needed for this test. See http://crbug.com/127381.
#if defined(OS_MACOSX)
#define MAYBE_NavigateClearsOrdinal DISABLED_NavigateClearsOrdinal
#else
#define MAYBE_NavigateClearsOrdinal NavigateClearsOrdinal
#endif

// This tests that the ordinal and match count is cleared after a navigation,
// as reported in issue http://crbug.com/126468.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, MAYBE_NavigateClearsOrdinal) {
  // First we navigate to our test content.
  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURL(browser(), url);

  // Open the Find box. In most tests we can just search without opening the
  // box first, but in this case we are testing functionality triggered by
  // NOTIFICATION_NAV_ENTRY_COMMITTED in the FindBarController and the observer
  // for that event isn't setup unless the box is open.
  EnsureFindBoxOpen();

  // Search for a text that exists within a link on the page.
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NULL != web_contents);
  int ordinal = 0;
  EXPECT_EQ(8, FindInPageWchar(web_contents,
                               L"e",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Then navigate away (to any page).
  url = GetURL(kLinkPage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Open the Find box again.
  EnsureFindBoxOpen();

  EXPECT_EQ(ASCIIToUTF16("e"), GetFindBarText());
  EXPECT_EQ(ASCIIToUTF16(""), GetMatchCountText());
}

// Load a page with no selectable text and make sure we don't crash.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindUnselectableText) {
  // First we navigate to our page.
  GURL url = GetURL(kUserSelectPage);
  ui_test_utils::NavigateToURL(browser(), url);

  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageWchar(web_contents,
                               L"text",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
}

// Try to reproduce the crash seen in issue 1341577.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindCrash_Issue1341577) {
  // First we navigate to our page.
  GURL url = GetURL(kCrashPage);
  ui_test_utils::NavigateToURL(browser(), url);

  // This would crash the tab. These must be the first two find requests issued
  // against the frame, otherwise an active frame pointer is set and it wont
  // produce the crash.
  // We used to check the return value and |ordinal|. With ICU 4.2, FiP does
  // not find a stand-alone dependent vowel sign of Indic scripts. So, the
  // exptected values are all 0. To make this test pass regardless of
  // ICU version, we just call FiP and see if there's any crash.
  // TODO(jungshik): According to a native Malayalam speaker, it's ok not
  // to find U+0D4C. Still need to investigate further this issue.
  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  FindInPageWchar(web_contents, L"\u0D4C", kFwd, kIgnoreCase, &ordinal);
  FindInPageWchar(web_contents, L"\u0D4C", kFwd, kIgnoreCase, &ordinal);

  // This should work fine.
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"\u0D24\u0D46",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(0, FindInPageWchar(web_contents, L"nostring",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);
}

// Try to reproduce the crash seen in http://crbug.com/14491, where an assert
// hits in the BitStack size comparison in WebKit.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindCrash_Issue14491) {
  // First we navigate to our page.
  GURL url = GetURL(kBitstackCrash);
  ui_test_utils::NavigateToURL(browser(), url);

  // This used to crash the tab.
  int ordinal = 0;
  EXPECT_EQ(0, FindInPageWchar(browser()->tab_strip_model()->
                                   GetActiveWebContents(),
                               L"s", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);
}

// Test to make sure Find does the right thing when restarting from a timeout.
// We used to have a problem where we'd stop finding matches when all of the
// following conditions were true:
// 1) The page has a lot of text to search.
// 2) The page contains more than one match.
// 3) It takes longer than the time-slice given to each Find operation (100
//    ms) to find one or more of those matches (so Find times out and has to try
//    again from where it left off).
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindRestarts_Issue1155639) {
  // First we navigate to our page.
  GURL url = GetURL(kTooFewMatchesPage);
  ui_test_utils::NavigateToURL(browser(), url);

  // This string appears 5 times at the bottom of a long page. If Find restarts
  // properly after a timeout, it will find 5 matches, not just 1.
  int ordinal = 0;
  EXPECT_EQ(5, FindInPageWchar(browser()->tab_strip_model()->
                                   GetActiveWebContents(),
                               L"008.xml",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
}

// Make sure we don't get into an infinite loop when text box contains very
// large amount of text.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindRestarts_Issue70505) {
  // First we navigate to our page.
  GURL url = GetURL(kLongTextareaPage);
  ui_test_utils::NavigateToURL(browser(), url);

  // If this test hangs on the FindInPage call, then it might be a regression
  // such as the one found in issue http://crbug.com/70505.
  int ordinal = 0;
  FindInPageWchar(browser()->tab_strip_model()->GetActiveWebContents(),
                  L"a", kFwd, kIgnoreCase, &ordinal);
  EXPECT_EQ(1, ordinal);
  // TODO(finnur): We cannot reliably get the matchcount for this Find call
  // until we fix issue http://crbug.com/71176.
}

// This tests bug 11761: FindInPage terminates search prematurely.
// This test is not expected to pass until bug 11761 is fixed.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       DISABLED_FindInPagePrematureEnd) {
  // First we navigate to our special focus tracking page.
  GURL url = GetURL(kPrematureEnd);
  ui_test_utils::NavigateToURL(browser(), url);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NULL != web_contents);

  // Search for a text that exists within a link on the page.
  int ordinal = 0;
  EXPECT_EQ(2, FindInPageWchar(web_contents, L"html ",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
}

IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindDisappearOnNavigate) {
  // First we navigate to our special focus tracking page.
  GURL url = GetURL(kSimple);
  GURL url2 = GetURL(kFramePage);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::ShowFindBar(browser());

  gfx::Point position;
  bool fully_visible = false;

  // Make sure it is open.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  // Reload the tab and make sure Find window doesn't go away.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  chrome::Reload(browser(), CURRENT_TAB);
  observer.Wait();

  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  // Navigate and make sure the Find window goes away.
  ui_test_utils::NavigateToURL(browser(), url2);

  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_FALSE(fully_visible);
}

IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindStayVisibleOnAnchorLoad) {
  // First we navigate to our special focus tracking page.
  GURL url = GetURL(kAnchorPage);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::ShowFindBar(browser());

  gfx::Point position;
  bool fully_visible = false;

  // Make sure it is open.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  // Navigate to the same page (but add an anchor/ref/fragment/whatever the kids
  // are calling it these days).
  GURL url_with_anchor = url.Resolve(kAnchor);
  ui_test_utils::NavigateToURL(browser(), url_with_anchor);

  // Make sure it is still open.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);
}

#if defined(OS_MACOSX)
// FindDisappearOnNewTabAndHistory is flaky, at least on Mac.
// See http://crbug.com/43072
#define FindDisappearOnNewTabAndHistory DISABLED_FindDisappearOnNewTabAndHistory
#endif

// Make sure Find box disappears when History/Downloads page is opened, and
// when a New Tab is opened.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       FindDisappearOnNewTabAndHistory) {
  // First we navigate to our special focus tracking page.
  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::ShowFindBar(browser());

  gfx::Point position;
  bool fully_visible = false;

  // Make sure it is open.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  // Open another tab (tab B).
  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), url);

  // Make sure Find box is closed.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_FALSE(fully_visible);

  // Close tab B.
  chrome::CloseTab(browser());

  // Make sure Find window appears again.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  chrome::ShowHistory(browser());

  // Make sure Find box is closed.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_FALSE(fully_visible);
}

// Make sure Find box moves out of the way if it is obscuring the active match.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindMovesWhenObscuring) {
  GURL url = GetURL(kMoveIfOver);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::ShowFindBar(browser());

  // This is needed on GTK because the reposition operation is asynchronous.
  MessageLoop::current()->RunUntilIdle();

  gfx::Point start_position;
  gfx::Point position;
  bool fully_visible = false;
  int ordinal = 0;

  // Make sure it is open.
  EXPECT_TRUE(GetFindBarWindowInfo(&start_position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  int moved_x_coord = FindInPageTillBoxMoves(web_contents, start_position.x(),
      L"Chromium", kMoveIterations);
  // The find box should have moved.
  EXPECT_TRUE(moved_x_coord != start_position.x());

  // Search for something guaranteed not to be obscured by the Find box.
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"Done",
                               kFwd, kIgnoreCase, &ordinal));
  // Check the position.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  // Make sure Find box has moved back to its original location.
  EXPECT_EQ(position.x(), start_position.x());

  // Move the find box again.
  moved_x_coord = FindInPageTillBoxMoves(web_contents, start_position.x(),
      L"Chromium", kMoveIterations);
  EXPECT_TRUE(moved_x_coord != start_position.x());

  // Search for an invalid string.
  EXPECT_EQ(0, FindInPageWchar(web_contents, L"WeirdSearchString",
                               kFwd, kIgnoreCase, &ordinal));

  // Check the position.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  // Make sure Find box has moved back to its original location.
  EXPECT_EQ(position.x(), start_position.x());
}

#if defined(OS_MACOSX)
// FindNextInNewTabUsesPrepopulate times-out, at least on Mac.
// See http://crbug.com/43070
#define MAYBE_FindNextInNewTabUsesPrepopulate \
    DISABLED_FindNextInNewTabUsesPrepopulate
#elif defined(OS_WIN) || defined(USE_AURA)
// Occasionally times-out on Windows or aura, too.
// See http://crbug.com/43070
#define MAYBE_FindNextInNewTabUsesPrepopulate \
    DISABLED_FindNextInNewTabUsesPrepopulate
#else
#define MAYBE_FindNextInNewTabUsesPrepopulate FindNextInNewTabUsesPrepopulate
#endif

// Make sure F3 in a new tab works if Find has previous string to search for.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       MAYBE_FindNextInNewTabUsesPrepopulate) {
  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for 'no_match'. No matches should be found.
  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(0, FindInPageWchar(web_contents, L"no_match",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);

  // Open another tab (tab B).
  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), url);

  // Simulate what happens when you press F3 for FindNext. We should get a
  // response here (a hang means search was aborted).
  EXPECT_EQ(0, ui_test_utils::FindInPage(web_contents, string16(),
                                         kFwd, kIgnoreCase, &ordinal, NULL));
  EXPECT_EQ(0, ordinal);

  // Open another tab (tab C).
  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), url);

  // Simulate what happens when you press F3 for FindNext. We should get a
  // response here (a hang means search was aborted).
  EXPECT_EQ(0, ui_test_utils::FindInPage(web_contents, string16(),
                                         kFwd, kIgnoreCase, &ordinal, NULL));
  EXPECT_EQ(0, ordinal);
}

// Make sure Find box does not become UI-inactive when no text is in the box as
// we switch to a tab contents with an empty find string. See issue 13570.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, StayActive) {
  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::ShowFindBar(browser());

  // Simulate a user clearing the search string. Ideally, we should be
  // simulating keypresses here for searching for something and pressing
  // backspace, but that's been proven flaky in the past, so we go straight to
  // web_contents.
  FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  // Stop the (non-existing) find operation, and clear the selection (which
  // signals the UI is still active).
  find_tab_helper->StopFinding(FindBarController::kClearSelectionOnPage);
  // Make sure the Find UI flag hasn't been cleared, it must be so that the UI
  // still responds to browser window resizing.
  ASSERT_TRUE(find_tab_helper->find_ui_active());
}

// Make sure F3 works after you FindNext a couple of times and end the Find
// session. See issue http://crbug.com/28306.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, RestartSearchFromF3) {
  // First we navigate to a simple page.
  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for 'page'. Should have 1 match.
  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"page",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Simulate what happens when you press F3 for FindNext. Still should show
  // one match. This cleared the pre-populate string at one point (see bug).
  EXPECT_EQ(1, ui_test_utils::FindInPage(web_contents, string16(),
                                         kFwd, kIgnoreCase, &ordinal, NULL));
  EXPECT_EQ(1, ordinal);

  // End the Find session, thereby making the next F3 start afresh.
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelectionOnPage,
      FindBarController::kKeepResultsInFindBox);

  // Simulate F3 while Find box is closed. Should have 1 match.
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
}

// When re-opening the find bar with F3, the find bar should be re-populated
// with the last search from the same tab rather than the last overall search.
// http://crbug.com/30006
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, PreferPreviousSearch) {
  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURL(browser(), url);

  // Find "Default".
  int ordinal = 0;
  WebContents* web_contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageWchar(web_contents_1, L"text",
                               kFwd, kIgnoreCase, &ordinal));

  // Create a second tab.
  // For some reason we can't use AddSelectedTabWithURL here on ChromeOS. It
  // could be some delicate assumption about the tab starting off unselected or
  // something relating to user gesture.
  chrome::AddBlankTabAt(browser(), -1, true);
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* web_contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(web_contents_1, web_contents_2);

  // Find "given".
  FindInPageWchar(web_contents_2, L"given", kFwd, kIgnoreCase, &ordinal);

  // Switch back to first tab.
  browser()->tab_strip_model()->ActivateTabAt(0, false);
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelectionOnPage,
      FindBarController::kKeepResultsInFindBox);
  // Simulate F3.
  ui_test_utils::FindInPage(web_contents_1, string16(),
                            kFwd, kIgnoreCase, &ordinal, NULL);
  EXPECT_EQ(FindTabHelper::FromWebContents(web_contents_1)->find_text(),
            WideToUTF16(L"text"));
}

// This tests that whenever you close and reopen the Find bar, it should show
// the last search entered in that tab. http://crbug.com/40121.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, PrepopulateSameTab) {
#if defined(OS_MACOSX)
  // FindInPage on Mac doesn't use prepopulated values. Search there is global.
  return;
#endif

  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for the word "page".
  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageWchar(web_contents, L"page",
                               kFwd, kIgnoreCase, &ordinal));

  // Open the Find box.
  EnsureFindBoxOpen();

  EXPECT_EQ(ASCIIToUTF16("page"), GetFindBarText());
  EXPECT_EQ(ASCIIToUTF16("1 of 1"), GetMatchCountText());

  // Close the Find box.
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelectionOnPage,
      FindBarController::kKeepResultsInFindBox);

  // Open the Find box again.
  EnsureFindBoxOpen();

  // After the Find box has been reopened, it should have been prepopulated with
  // the word "page" again.
  EXPECT_EQ(ASCIIToUTF16("page"), GetFindBarText());
  EXPECT_EQ(ASCIIToUTF16("1 of 1"), GetMatchCountText());
}

// This tests that whenever you open Find in a new tab it should prepopulate
// with a previous search term (in any tab), if a search has not been issued in
// this tab before.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, PrepopulateInNewTab) {
#if defined(OS_MACOSX)
  // FindInPage on Mac doesn't use prepopulated values. Search there is global.
  return;
#endif

  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for the word "page".
  int ordinal = 0;
  WebContents* web_contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageWchar(web_contents_1, L"page",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(ASCIIToUTF16("1 of 1"), GetMatchCountText());

  // Now create a second tab and load the same page.
  chrome::AddSelectedTabWithURL(browser(), url, content::PAGE_TRANSITION_TYPED);
  WebContents* web_contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(web_contents_1, web_contents_2);

  // Open the Find box.
  EnsureFindBoxOpen();

  // The new tab should have "page" prepopulated, since that was the last search
  // in the first tab.
  EXPECT_EQ(ASCIIToUTF16("page"), GetFindBarText());
  // But it should not seem like a search has been issued.
  EXPECT_EQ(string16(), GetMatchCountText());
}

// This makes sure that we can search for A in tabA, then for B in tabB and
// when we come back to tabA we should still see A (because that was the last
// search in that tab).
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, PrepopulatePreserveLast) {
#if defined(OS_MACOSX)
  // FindInPage on Mac doesn't use prepopulated values. Search there is global.
  return;
#endif

  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for the word "page".
  int ordinal = 0;
  WebContents* web_contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageWchar(web_contents_1, L"page",
                               kFwd, kIgnoreCase, &ordinal));

  // Open the Find box.
  EnsureFindBoxOpen();

  EXPECT_EQ(ASCIIToUTF16("page"), GetFindBarText());

  // Close the Find box.
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelectionOnPage,
      FindBarController::kKeepResultsInFindBox);

  // Now create a second tab and load the same page.
  chrome::AddBlankTabAt(browser(), -1, true);
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* web_contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(web_contents_1, web_contents_2);

  // Search for the word "text".
  FindInPageWchar(web_contents_2, L"text", kFwd, kIgnoreCase, &ordinal);

  // Go back to the first tab and make sure we have NOT switched the prepopulate
  // text to "text".
  browser()->tab_strip_model()->ActivateTabAt(0, false);

  // Open the Find box.
  EnsureFindBoxOpen();

  // After the Find box has been reopened, it should have been prepopulated with
  // the word "page" again, since that was the last search in that tab.
  EXPECT_EQ(ASCIIToUTF16("page"), GetFindBarText());

  // Close the Find box.
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelectionOnPage,
      FindBarController::kKeepResultsInFindBox);

  // Re-open the Find box.
  // This is a special case: previous search in WebContents used to get cleared
  // if you opened and closed the FindBox, which would cause the global
  // prepopulate value to show instead of last search in this tab.
  EnsureFindBoxOpen();

  // After the Find box has been reopened, it should have been prepopulated with
  // the word "page" again, since that was the last search in that tab.
  EXPECT_EQ(ASCIIToUTF16("page"), GetFindBarText());
}

// TODO(rohitrao): Searching in incognito tabs does not work in browser tests in
// Linux views.  Investigate and fix.  http://crbug.com/40948
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
#define MAYBE_NoIncognitoPrepopulate DISABLED_NoIncognitoPrepopulate
#else
#define MAYBE_NoIncognitoPrepopulate NoIncognitoPrepopulate
#endif

// This tests that search terms entered into an incognito find bar are not used
// as prepopulate terms for non-incognito windows.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, MAYBE_NoIncognitoPrepopulate) {
#if defined(OS_MACOSX)
  // FindInPage on Mac doesn't use prepopulated values. Search there is global.
  return;
#endif

  // First we navigate to the "simple" test page.
  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for the word "page" in the normal browser tab.
  int ordinal = 0;
  WebContents* web_contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageWchar(web_contents_1, L"page",
                               kFwd, kIgnoreCase, &ordinal));

  // Open the Find box.
  EnsureFindBoxOpenForBrowser(browser());
  EXPECT_EQ(ASCIIToUTF16("page"), GetFindBarTextForBrowser(browser()));

  // Close the Find box.
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelectionOnPage,
      FindBarController::kKeepResultsInFindBox);

  // Open a new incognito window and navigate to the same page.
  Profile* incognito_profile = browser()->profile()->GetOffTheRecordProfile();
  Browser* incognito_browser =
      new Browser(Browser::CreateParams(incognito_profile));
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(incognito_browser, url,
                                content::PAGE_TRANSITION_AUTO_TOPLEVEL);
  observer.Wait();
  incognito_browser->window()->Show();

  // Open the find box and make sure that it is prepopulated with "page".
  EnsureFindBoxOpenForBrowser(incognito_browser);
  EXPECT_EQ(ASCIIToUTF16("page"), GetFindBarTextForBrowser(incognito_browser));

  // Search for the word "text" in the incognito tab.
  WebContents* incognito_tab =
      incognito_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageWchar(incognito_tab, L"text",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(ASCIIToUTF16("text"), GetFindBarTextForBrowser(incognito_browser));

  // Close the Find box.
  incognito_browser->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelectionOnPage,
      FindBarController::kKeepResultsInFindBox);

  // Now open a new tab in the original (non-incognito) browser.
  chrome::AddSelectedTabWithURL(browser(), url, content::PAGE_TRANSITION_TYPED);
  WebContents* web_contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(web_contents_1, web_contents_2);

  // Open the Find box and make sure it is prepopulated with the search term
  // from the original browser, not the search term from the incognito window.
  EnsureFindBoxOpenForBrowser(browser());
  EXPECT_EQ(ASCIIToUTF16("page"), GetFindBarTextForBrowser(browser()));
}

// This makes sure that dismissing the find bar with kActivateSelection works.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, ActivateLinkNavigatesPage) {
  // First we navigate to our test content.
  GURL url = GetURL(kLinkPage);
  ui_test_utils::NavigateToURL(browser(), url);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  FindTabHelper* find_tab_helper =
      FindTabHelper::FromWebContents(web_contents);

  int ordinal = 0;
  FindInPageWchar(web_contents, L"link", kFwd, kIgnoreCase, &ordinal);
  EXPECT_EQ(ordinal, 1);

  // End the find session, click on the link.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(&web_contents->GetController()));
  find_tab_helper->StopFinding(FindBarController::kActivateSelectionOnPage);
  observer.Wait();
}

// Tests that FindBar fits within a narrow browser window.
// Flaky on Linux/GTK: http://crbug.com/136443.
#if defined(TOOLKIT_GTK)
#define MAYBE_FitWindow DISABLED_FitWindow
#else
#define MAYBE_FitWindow FitWindow
#endif
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, MAYBE_FitWindow) {
  Browser::CreateParams params(Browser::TYPE_POPUP, browser()->profile());
  params.initial_bounds = gfx::Rect(0, 0, 250, 500);
  Browser* popup = new Browser(params);
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(popup, GURL(chrome::kAboutBlankURL),
                                content::PAGE_TRANSITION_LINK);
  // Wait for the page to finish loading.
  observer.Wait();
  popup->window()->Show();

  // On GTK, bounds change is asynchronous.
  MessageLoop::current()->RunUntilIdle();

  EnsureFindBoxOpenForBrowser(popup);

  // GTK adjusts FindBar size asynchronously.
  MessageLoop::current()->RunUntilIdle();

  ASSERT_LE(GetFindBarWidthForBrowser(popup),
            popup->window()->GetBounds().width());
}

IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       FindMovesOnTabClose_Issue1343052) {
  EnsureFindBoxOpen();
  content::RunAllPendingInMessageLoop();  // Needed on Linux.

  gfx::Point position;
  EXPECT_TRUE(GetFindBarWindowInfo(&position, NULL));

  // Open another tab.
  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Close it.
  chrome::CloseTab(browser());

  // See if the Find window has moved.
  gfx::Point position2;
  EXPECT_TRUE(GetFindBarWindowInfo(&position2, NULL));
  EXPECT_EQ(position, position2);

  // Toggle the bookmark bar state. Note that this starts an animation, and
  // there isn't a good way other than looping and polling to see when it's
  // done. So instead we change the state and open a new tab, since the new tab
  // animation doesn't happen on tab change.
  chrome::ToggleBookmarkBar(browser());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  EnsureFindBoxOpen();
  content::RunAllPendingInMessageLoop();  // Needed on Linux.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, NULL));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  chrome::CloseTab(browser());
  EXPECT_TRUE(GetFindBarWindowInfo(&position2, NULL));
  EXPECT_EQ(position, position2);
}
