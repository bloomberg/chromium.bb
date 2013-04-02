// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/rand_util.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/browser/webdata/webdata_constants.h"
#include "chrome/common/url_constants.h"
#include "chrome_frame/test/mock_ie_event_sink_actions.h"
#include "chrome_frame/test/mock_ie_event_sink_test.h"

using testing::_;

namespace chrome_frame_test {

class DeleteBrowsingHistoryTest
    : public MockIEEventSinkTest,
      public testing::Test {
 public:
  DeleteBrowsingHistoryTest() {}

  virtual void SetUp() {
    // We will use the OnAccLoad event to monitor page loads, so we ignore
    // these.
    ie_mock_.ExpectAnyNavigations();
    ie_mock2_.ExpectAnyNavigations();
    ie_mock3_.ExpectAnyNavigations();
    EXPECT_CALL(acc_observer_, OnAccDocLoad(_)).Times(testing::AnyNumber());

    // Use a random image_path to ensure that a prior run does not
    // interfere with our expectations about caching.
    image_path_ = L"/" + RandomChars(32);
    topHtml =
        "<html><head>"
        "<meta http-equiv=\"x-ua-compatible\" content=\"chrome=1\" />"
        "</head>"
        "<body>"
        "<form method=\"POST\" action=\"/form\">"
        "<input title=\"username\" type=\"text\" name=\"username\" />"
        "<input type=\"submit\" title=\"Submit\" name=\"Submit\" />"
        "</form>"
        "<img alt=\"Blank image.\" src=\"" + WideToASCII(image_path_) + "\" />"
        "This is some text.</body></html>";
  }

 protected:
  std::wstring image_path_;
  std::string topHtml;

  testing::NiceMock<MockAccEventObserver> acc_observer_;
  MockWindowObserver delete_browsing_history_window_observer_mock_;
  MockObjectWatcherDelegate ie_process_exit_watcher_mock_;

  testing::StrictMock<MockIEEventSink> ie_mock2_;
  testing::StrictMock<MockIEEventSink> ie_mock3_;

  // Returns a string of |count| lowercase random characters.
  static std::wstring RandomChars(int count) {
    srand(static_cast<unsigned int>(time(NULL)));
    std::wstring str;
    for (int i = 0; i < count; ++i)
      str += L'a' + base::RandInt(0, 25);
    return str;
  }
};

namespace {

const wchar_t* kFormFieldName = L"username";
const wchar_t* kFormFieldValue = L"test_username";

const char* kHtmlHttpHeaders =
    "HTTP/1.1 200 OK\r\n"
    "Connection: close\r\n"
    "Content-Type: text/html\r\n";
const char* kFormResultHtml =
    "<html><head><meta http-equiv=\"x-ua-compatible\" content=\"chrome=1\" />"
    "</head><body>Nice work.</body></html>";
const char* kBlankPngResponse[] = {
    "HTTP/1.1 200 OK\r\n"
    "Connection: close\r\n"
    "Content-Type: image/png\r\n"
    "Cache-Control: max-age=3600, must-revalidate\r\n",
    "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52"
    "\x00\x00\x00\x01\x00\x00\x00\x01\x01\x03\x00\x00\x00\x25\xdb\x56"
    "\xca\x00\x00\x00\x03\x50\x4c\x54\x45\x00\x00\x00\xa7\x7a\x3d\xda"
    "\x00\x00\x00\x01\x74\x52\x4e\x53\x00\x40\xe6\xd8\x66\x00\x00\x00"
    "\x0a\x49\x44\x41\x54\x08\xd7\x63\x60\x00\x00\x00\x02\x00\x01\xe2"
    "\x21\xbc\x33\x00\x00\x00\x00\x49\x45\x4e\x44\xae\x42\x60\x82"};

const size_t kBlankPngFileLength = 95;
}  // anonymous namespace

// Looks up |element_name| in the Chrome form data DB and ensures that the
// results match |matcher|.
ACTION_P2(ExpectFormValuesForElementNameMatch, element_name, matcher) {
  base::FilePath root_path;
  GetChromeFrameProfilePath(kIexploreProfileName, &root_path);
  base::FilePath profile_path(
      root_path.Append(L"Default").Append(kWebDataFilename));

  AutofillTable autofill_table;
  WebDatabase web_database;
  web_database.AddTable(&autofill_table);
  sql::InitStatus init_status = web_database.Init(profile_path);
  EXPECT_EQ(sql::INIT_OK, init_status);

  if (init_status == sql::INIT_OK) {
    std::vector<string16> values;
    autofill_table.GetFormValuesForElementName(
        element_name, L"", &values, 9999);
    EXPECT_THAT(values, matcher);
  }
}

// Launch |ie_mock| and navigate it to |url|.
ACTION_P2(LaunchThisIEAndNavigate, ie_mock, url) {
  EXPECT_HRESULT_SUCCEEDED(ie_mock->event_sink()->LaunchIEAndNavigate(url,
                                                                      ie_mock));
}

// Listens for OnAccLoad and OnLoad events for an IE instance and
// sends a single signal once both have been received.
//
// Allows tests to wait for both events to occur irrespective of their relative
// ordering.
class PageLoadHelper {
 public:
  explicit PageLoadHelper(testing::StrictMock<MockIEEventSink>* ie_mock)
      : received_acc_load_(false),
        received_on_load_(false),
        ie_mock_(ie_mock) {
    EXPECT_CALL(*ie_mock_, OnLoad(_, _))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::InvokeWithoutArgs(
            this, &PageLoadHelper::HandleOnLoad));
    EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Invoke(this, &PageLoadHelper::HandleAccLoad));
  }

  void HandleAccLoad(HWND hwnd) {
    ReconcileHwnds(hwnd, &acc_loaded_hwnds_, &on_loaded_hwnds_);
  }

  void HandleOnLoad() {
    HWND hwnd = ie_mock_->event_sink()->GetRendererWindow();
    ReconcileHwnds(hwnd, &on_loaded_hwnds_, &acc_loaded_hwnds_);
  }

  MOCK_METHOD0(OnLoadComplete, void());

 private:
  void ReconcileHwnds(HWND signaled_hwnd,
                      std::set<HWND>* signaled_hwnd_set,
                      std::set<HWND>* other_hwnd_set) {
    if (other_hwnd_set->erase(signaled_hwnd) != 0) {
      OnLoadComplete();
    } else {
      signaled_hwnd_set->insert(signaled_hwnd);
    }
  }
  std::set<HWND> acc_loaded_hwnds_;
  std::set<HWND> on_loaded_hwnds_;
  bool received_acc_load_;
  bool received_on_load_;
  testing::StrictMock<MockIEEventSink>* ie_mock_;
  testing::NiceMock<MockAccEventObserver> acc_observer_;
};

TEST_F(DeleteBrowsingHistoryTest, DISABLED_CFDeleteBrowsingHistory) {
  if (GetInstalledIEVersion() < IE_8) {
    LOG(ERROR) << "Test does not apply to IE versions < 8.";
    return;
  }

  PageLoadHelper load_helper(&ie_mock_);
  PageLoadHelper load_helper2(&ie_mock2_);
  PageLoadHelper load_helper3(&ie_mock3_);

  delete_browsing_history_window_observer_mock_.WatchWindow(
      "Delete Browsing History", "");

  // For some reason, this page is occasionally being cached, so we randomize
  // its name to ensure that, at least the first time we request it, it is
  // retrieved.
  std::wstring top_name = RandomChars(32);
  std::wstring top_url = server_mock_.Resolve(top_name);
  std::wstring top_path = L"/" + top_name;

  // Even still, it might not be hit the second or third time, so let's just
  // not worry about how often or whether it's called
  EXPECT_CALL(server_mock_, Get(_, testing::StrEq(top_path), _))
      .Times(testing::AnyNumber())
      .WillRepeatedly(SendFast(kHtmlHttpHeaders, topHtml));

  testing::InSequence expect_in_sequence_for_scope;

  // First launch will hit the server, requesting top.html and then image_path_
  EXPECT_CALL(server_mock_, Get(_, testing::StrEq(image_path_), _))
      .WillOnce(SendFast(kBlankPngResponse[0],
                         std::string(kBlankPngResponse[1],
                                     kBlankPngFileLength)));

  // top.html contains a form. Fill in the username field and submit, causing
  // the value to be stored in Chrome's form data DB.
  EXPECT_CALL(load_helper, OnLoadComplete())
      .WillOnce(testing::DoAll(
          AccLeftClickInRenderer(&ie_mock_, AccObjectMatcher(L"username")),
          PostCharMessagesToRenderer(&ie_mock_, WideToASCII(kFormFieldValue)),
          AccLeftClickInRenderer(&ie_mock_, AccObjectMatcher(L"Submit"))));

  EXPECT_CALL(server_mock_, Post(_, testing::StrEq(L"/form"), _))
      .WillOnce(SendFast(kHtmlHttpHeaders, kFormResultHtml));

  // OnLoad of the result page from form submission. Now close the browser.
  EXPECT_CALL(load_helper, OnLoadComplete())
      .WillOnce(testing::DoAll(
          WatchRendererProcess(&ie_process_exit_watcher_mock_, &ie_mock_),
          CloseBrowserMock(&ie_mock_)));

  EXPECT_CALL(ie_mock_, OnQuit());

  // Wait until the process is gone, so that the Chrome databases are unlocked.
  // Verify that the submitted username is in the database, then launch a new
  // IE instance.
  EXPECT_CALL(ie_process_exit_watcher_mock_, OnObjectSignaled(_))
      .WillOnce(testing::DoAll(
          ExpectFormValuesForElementNameMatch(
            kFormFieldName, testing::Contains(kFormFieldValue)),
          LaunchThisIEAndNavigate(&ie_mock2_, top_url)));

  // Second launch won't load the image due to the cache.

  // We do the delete private data twice, each time toggling the state of the
  // 'Delete form data' and 'Delete temporary files' options.
  // That's because we have no way to know their initial states. Using this,
  // trick we are guaranteed to run it exactly once with each option turned on.
  // Running it once with the option turned off is harmless.

  // Proceed to open up the "Safety" menu for the first time through the loop.
  EXPECT_CALL(load_helper2, OnLoadComplete())
      .WillOnce(AccDoDefaultActionInBrowser(&ie_mock2_,
                                            AccObjectMatcher(L"Safety")));

  // Store the dialog and progress_bar HWNDs for each iteration
  // in order to distinguish between the OnClose of each.
  HWND dialog[] = {NULL, NULL};
  HWND progress_bar[] = {NULL, NULL};

  for (int i = 0; i < 2; ++i) {
    // Watch for the popup menu, click 'Delete Browsing History...'
    EXPECT_CALL(acc_observer_, OnMenuPopup(_))
        .WillOnce(
            AccLeftClick(AccObjectMatcher(L"Delete Browsing History...*")));

    // When it shows up, toggle the options and click "Delete".
    EXPECT_CALL(delete_browsing_history_window_observer_mock_, OnWindowOpen(_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<0>(&dialog[i]),
            AccLeftClick(AccObjectMatcher(L"Temporary Internet files")),
            AccLeftClick(AccObjectMatcher(L"Form data")),
            AccLeftClick(AccObjectMatcher(L"Delete"))));

    // The configuration dialog closes.
    // This is not reliably ordered with respect to the following OnWindowOpen.
    // Specifying 'AnyNumber' of times allows us to disregard it, although we
    // can't avoid receiving the call.
    EXPECT_CALL(delete_browsing_history_window_observer_mock_,
                OnWindowClose(testing::Eq(testing::ByRef(dialog[i]))))
        .Times(testing::AnyNumber());

    // The progress dialog that pops up has the same caption.
    EXPECT_CALL(delete_browsing_history_window_observer_mock_,
        OnWindowOpen(_)).WillOnce(testing::SaveArg<0>(&progress_bar[i]));

    // Watch for it to go away, then either do the "Delete History" again or
    // close the browser.
    // In either case, validate the contents of the renderer to ensure that
    // we didn't cause Chrome to crash.
    if (i == 0) {
      EXPECT_CALL(delete_browsing_history_window_observer_mock_,
          OnWindowClose(testing::Eq(testing::ByRef(progress_bar[i]))))
          .WillOnce(testing::DoAll(
              AccExpectInRenderer(&ie_mock2_,
                                  AccObjectMatcher(L"Blank image.")),
              AccDoDefaultActionInBrowser(&ie_mock2_,
                                          AccObjectMatcher(L"Safety"))));
    } else {
      EXPECT_CALL(delete_browsing_history_window_observer_mock_,
        OnWindowClose(testing::Eq(testing::ByRef(progress_bar[i]))))
          .WillOnce(testing::DoAll(
              AccExpectInRenderer(&ie_mock2_,
                                  AccObjectMatcher(L"Blank image.")),
              WatchRendererProcess(&ie_process_exit_watcher_mock_,
                                   &ie_mock2_),
              CloseBrowserMock(&ie_mock2_)));
    }
  }

  EXPECT_CALL(ie_mock2_, OnQuit());

  // When the process is actually exited, and the DB has been released, verify
  // that the remembered form data is not in the form data DB.
  EXPECT_CALL(ie_process_exit_watcher_mock_, OnObjectSignaled(_))
      .WillOnce(testing::DoAll(
          ExpectFormValuesForElementNameMatch(
              kFormFieldName, testing::Not(testing::Contains(kFormFieldValue))),
          LaunchThisIEAndNavigate(&ie_mock3_, top_url)));

  // Now that the cache is cleared, final session should load the image from the
  // server.
  EXPECT_CALL(server_mock_, Get(_, testing::StrEq(image_path_), _))
      .WillOnce(
          SendFast(kBlankPngResponse[0], std::string(kBlankPngResponse[1],
                                                     kBlankPngFileLength)));

  EXPECT_CALL(load_helper3, OnLoadComplete())
    .WillOnce(CloseBrowserMock(&ie_mock3_));

  EXPECT_CALL(ie_mock3_, OnQuit())
      .WillOnce(QUIT_LOOP(loop_));

  // Start it up. Everything else is triggered as mock actions.
  ASSERT_HRESULT_SUCCEEDED(
      ie_mock_.event_sink()->LaunchIEAndNavigate(top_url, &ie_mock_));

  // 3 navigations + 2 invocations of delete browser history == 5
  loop_.RunFor(kChromeFrameLongNavigationTimeout * 5);
}

}  // namespace chrome_frame_test
