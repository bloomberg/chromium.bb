// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_CHROME_FRAME_UNITTESTS_H_
#define CHROME_FRAME_TEST_CHROME_FRAME_UNITTESTS_H_

#include <atlbase.h>
#include <atlcom.h>
#include <string>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>
#include <shlguid.h>
#include <shobjidl.h>

#include "base/compiler_specific.h"
#include "base/ref_counted.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_variant_win.h"
#include "base/scoped_handle_win.h"
#include "googleurl/src/gurl.h"
#include "chrome_frame/test/http_server.h"
#include "chrome_frame/test_utils.h"
#include "chrome_frame/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

// Class that:
// 1) Starts the local webserver,
// 2) Supports launching browsers - Internet Explorer and Firefox with local url
// 3) Wait the webserver to finish. It is supposed the test webpage to shutdown
//    the server by navigating to "kill" page
// 4) Supports read the posted results from the test webpage to the "dump"
//    webserver directory
class ChromeFrameTestWithWebServer: public testing::Test {
 protected:
  enum BrowserKind { INVALID, IE, FIREFOX, OPERA, SAFARI, CHROME };

  bool LaunchBrowser(BrowserKind browser, const wchar_t* url);
  bool WaitForTestToComplete(int milliseconds);
  // Waits for the page to notify us of the window.onload event firing.
  // Note that the milliseconds value is only approximate.
  bool WaitForOnLoad(int milliseconds);
  bool ReadResultFile(const std::wstring& file_name, std::string* data);

  // Launches the specified browser and waits for the test to complete
  // (see WaitForTestToComplete).  Then checks that the outcome is OK.
  // This function uses EXPECT_TRUE and ASSERT_TRUE for all steps performed
  // hence no return value.
  void SimpleBrowserTest(BrowserKind browser, const wchar_t* page,
                         const wchar_t* result_file_to_check);

  // Same as SimpleBrowserTest but if the browser isn't installed (LaunchBrowser
  // fails), the function will print out a warning but not treat the test
  // as failed.
  // Currently this is how we run Opera tests.
  void OptionalBrowserTest(BrowserKind browser, const wchar_t* page,
                           const wchar_t* result_file_to_check);

  // Test if chrome frame correctly reports its version.
  void VersionTest(BrowserKind browser, const wchar_t* page,
                   const wchar_t* result_file_to_check);

  // Closes all browsers in preparation for a test and during cleanup.
  void CloseAllBrowsers();

  void CloseBrowser();

  // Ensures (well, at least tries to ensure) that the browser window has focus.
  bool BringBrowserToTop();

  // Returns true iff the specified result file contains 'expected result'.
  bool CheckResultFile(const std::wstring& file_name,
                       const std::string& expected_result);

  virtual void SetUp();
  virtual void TearDown();

  // Important: kind means "sheep" in Icelandic. ?:-o
  const char* ToString(BrowserKind kind) {
    switch (kind) {
      case IE:
        return "IE";
      case FIREFOX:
        return "Firefox";
      case OPERA:
        return "Opera";
      case CHROME:
        return "Chrome";
      case SAFARI:
        return "Safari";
      default:
        NOTREACHED();
        break;
    }
    return "";
  }

  BrowserKind browser_;
  std::wstring results_dir_;
  ScopedHandle browser_handle_;
  ChromeFrameHTTPServer server_;
};

#endif  // CHROME_FRAME_TEST_CHROME_FRAME_UNITTESTS_H_

