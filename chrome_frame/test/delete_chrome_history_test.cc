// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/rand_util.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/chrome_constants.h"
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
    EXPECT_CALL(ie_mock_, OnLoad(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(ie_mock2_, OnLoad(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(ie_mock3_, OnLoad(_, _)).Times(testing::AnyNumber());

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

 private:
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
  FilePath profile_path(
      chrome_frame_test::GetProfilePath(kIexploreProfileName)
          .Append(L"Default").Append(chrome::kWebDataFilename));

  WebDatabase web_database;
  sql::InitStatus init_status = web_database.Init(profile_path);
  EXPECT_EQ(sql::INIT_OK, init_status);

  if (init_status == sql::INIT_OK) {
    std::vector<string16> values;
    web_database.GetFormValuesForElementName(element_name, L"", &values, 9999);
    EXPECT_THAT(values, matcher);
  }
}

// Launch |ie_mock| and navigate it to |url|.
ACTION_P2(LaunchThisIEAndNavigate, ie_mock, url) {
  EXPECT_HRESULT_SUCCEEDED(ie_mock->event_sink()->LaunchIEAndNavigate(url,
                                                                      ie_mock));
}

TEST_F(DeleteBrowsingHistoryTest, CFDeleteBrowsingHistory) {
  if (GetInstalledIEVersion() < IE_8) {
    LOG(ERROR) << "Test does not apply to IE versions < 8.";
    return;
  }
  delete_browsing_history_window_observer_mock_.WatchWindow(
      "Delete Browsing History");

  EXPECT_CALL(server_mock_, Get(_, testing::StrEq(L"/top.html"), _))
      .WillRepeatedly(SendFast(kHtmlHttpHeaders, topHtml));
  EXPECT_CALL(server_mock_, Post(_, testing::StrEq(L"/form"), _))
      .WillRepeatedly(SendFast(kHtmlHttpHeaders, kFormResultHtml));

  std::wstring top_url = server_mock_.Resolve(L"top.html");

  testing::InSequence expect_in_sequence_for_scope;

  // First launch will hit the server, requesting top.html and then image_path_
  EXPECT_CALL(server_mock_, Get(_, testing::StrEq(image_path_), _))
      .WillOnce(SendFast(kBlankPngResponse[0],
                         std::string(kBlankPngResponse[1],
                                     kBlankPngFileLength)));

  // top.html contains a form. Fill in the username field and submit, causing
  // the value to be stored in Chrome's form data DB.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          AccLeftClickInRenderer(&ie_mock_, AccObjectMatcher(L"username")),
          PostCharMessagesToRenderer(&ie_mock_, WideToASCII(kFormFieldValue)),
          AccLeftClickInRenderer(&ie_mock_, AccObjectMatcher(L"Submit"))));

  // OnLoad of the result page from form submission. Now close the browser.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          WatchRendererProcess(&ie_process_exit_watcher_mock_, &ie_mock_),
          CloseBrowserMock(&ie_mock_)));

  // Wait until the process is gone, so that the Chrome databases are unlocked.
  EXPECT_CALL(ie_mock_, OnQuit());

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
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(AccDoDefaultActionInBrowser(&ie_mock2_,
                                            AccObjectMatcher(L"Safety")));

  for (int i = 0; i < 2; ++i) {
    // Watch for the popup menu, click 'Delete Browsing History...'
    EXPECT_CALL(acc_observer_, OnMenuPopup(_)).WillOnce(
        AccLeftClick(AccObjectMatcher(L"Delete Browsing History...*")));

    // When it shows up, toggle the options and click "Delete".
    EXPECT_CALL(delete_browsing_history_window_observer_mock_, OnWindowOpen(_))
        .WillOnce(testing::DoAll(
            AccLeftClick(AccObjectMatcher(L"Temporary Internet files")),
            AccLeftClick(AccObjectMatcher(L"Form data")),
            AccLeftClick(AccObjectMatcher(L"Delete"))));

    // The configuration dialog closes.
    EXPECT_CALL(delete_browsing_history_window_observer_mock_,
        OnWindowClose(_));

    // The progress dialog that pops up has the same caption.
    EXPECT_CALL(delete_browsing_history_window_observer_mock_,
        OnWindowOpen(_));

    // Watch for it to go away, then close the browser.
    if (i == 0) {
      EXPECT_CALL(delete_browsing_history_window_observer_mock_,
          OnWindowClose(_)).WillOnce(testing::DoAll(
              AccExpectInRenderer(&ie_mock2_,
                                  AccObjectMatcher(L"Blank image.")),
              AccDoDefaultActionInBrowser(&ie_mock2_,
                                          AccObjectMatcher(L"Safety"))));
    } else {
      EXPECT_CALL(delete_browsing_history_window_observer_mock_,
          OnWindowClose(_)).WillOnce(testing::DoAll(
              AccExpectInRenderer(&ie_mock2_,
                                  AccObjectMatcher(L"Blank image.")),
              CloseBrowserMock(&ie_mock2_)));
    }
  }

  // Close IE, wait for the process to exit...
  EXPECT_CALL(ie_mock2_, OnQuit())
      .WillOnce(WatchBrowserProcess(&ie_process_exit_watcher_mock_,
                                    &ie_mock2_));

  // ... and verify that the remembered form data is not in the form data DB.
  EXPECT_CALL(ie_process_exit_watcher_mock_, OnObjectSignaled(_))
      .WillOnce(testing::DoAll(
          ExpectFormValuesForElementNameMatch(
              kFormFieldName, testing::Not(testing::Contains(kFormFieldValue))),
          LaunchThisIEAndNavigate(&ie_mock3_, top_url)));

  // Now that the cache is cleared, final session should load the image from the
  // server.
  EXPECT_CALL(server_mock_, Get(_, testing::StrEq(image_path_), _)).WillOnce(
      SendFast(kBlankPngResponse[0], std::string(kBlankPngResponse[1],
                                                 kBlankPngFileLength)));
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(CloseBrowserMock(&ie_mock3_));
  EXPECT_CALL(ie_mock3_, OnQuit()).WillOnce(QUIT_LOOP(loop_));

  // Start it up. Everything else is triggered as mock actions.
  ASSERT_HRESULT_SUCCEEDED(
      ie_mock_.event_sink()->LaunchIEAndNavigate(top_url, &ie_mock_));

  // 3 navigations + 2 invocations of delete browser history == 5
  loop_.RunFor(kChromeFrameLongNavigationTimeoutInSeconds * 5);
}

}  // namespace chrome_frame_test
