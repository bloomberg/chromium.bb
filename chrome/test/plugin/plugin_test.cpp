// Copyright 2008-2009, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Tests for the top plugins to catch regressions in our plugin host code, as
// well as in the out of process code.  Currently this tests:
//  Flash
//  Real
//  QuickTime
//  Windows Media Player
//    -this includes both WMP plugins.  npdsplay.dll is the older one that
//     comes with XP.  np-mswmp.dll can be downloaded from Microsoft and
//     needs SP2 or Vista.

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <comutil.h>

#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/registry.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "third_party/npapi/bindings/npapi.h"
#include "webkit/default_plugin/plugin_impl.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_list.h"

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
const int kShortWaitTimeout = 10 * 1000;
const int kLongWaitTimeout  = 30 * 1000;

class PluginTest : public UITest {
 protected:
  virtual void SetUp() {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(), "MediaPlayerNew") == 0) {
      // The installer adds our process names to the registry key below.  Since
      // the installer might not have run on this machine, add it manually.
      RegKey regkey;
      if (regkey.Open(HKEY_LOCAL_MACHINE,
                      L"Software\\Microsoft\\MediaPlayer\\ShimInclusionList",
                      KEY_WRITE)) {
        regkey.CreateKey(L"CHROME.EXE", KEY_READ);
      }
    } else if (strcmp(test_info->name(), "MediaPlayerOld") == 0) {
      // When testing the old WMP plugin, we need to force Chrome to not load
      // the new plugin.
      launch_arguments_.AppendSwitch(kUseOldWMPPluginSwitch);
    } else if (strcmp(test_info->name(), "FlashSecurity") == 0) {
      launch_arguments_.AppendSwitchWithValue(switches::kTestSandbox,
                                              L"security_tests.dll");
    }

    UITest::SetUp();
  }

  void TestPlugin(const std::wstring& test_case,
                  int timeout,
                  bool mock_http) {
    GURL url = GetTestUrl(test_case, mock_http);
    NavigateToURL(url);
    WaitForFinish(timeout, mock_http);
  }

  // Generate the URL for testing a particular test.
  // HTML for the tests is all located in test_directory\plugin\<testcase>
  // Set |mock_http| to true to use mock HTTP server.
  GURL GetTestUrl(const std::wstring &test_case, bool mock_http) {
    if (mock_http) {
      return URLRequestMockHTTPJob::GetMockUrl(
                 FilePath(L"plugin/" + test_case));
    }

    FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("plugin");
    path = path.Append(FilePath::FromWStringHack(test_case));
    return net::FilePathToFileURL(path);
  }

  // Waits for the test case to finish.
  void WaitForFinish(const int wait_time, bool mock_http) {
    const int kSleepTime = 500;      // 2 times per second
    const int kMaxIntervals = wait_time / kSleepTime;

    GURL url = GetTestUrl(L"done", mock_http);
    scoped_refptr<TabProxy> tab(GetActiveTab());

    std::string done_str;
    for (int i = 0; i < kMaxIntervals; ++i) {
      Sleep(kSleepTime);

      // The webpage being tested has javascript which sets a cookie
      // which signals completion of the test.
      std::string cookieName = kTestCompleteCookie;
      tab->GetCookieByName(url, cookieName, &done_str);
      if (!done_str.empty())
        break;
    }

    EXPECT_EQ(kTestCompleteSuccess, done_str);
  }
};

TEST_F(PluginTest, Quicktime) {
  TestPlugin(L"quicktime.html", kShortWaitTimeout, false);
}

TEST_F(PluginTest, MediaPlayerNew) {
  TestPlugin(L"wmp_new.html", kShortWaitTimeout, false);
}

// http://crbug.com/4809
TEST_F(PluginTest, DISABLED_MediaPlayerOld) {
  TestPlugin(L"wmp_old.html", kLongWaitTimeout, false);
}

TEST_F(PluginTest, Real) {
  TestPlugin(L"real.html", kShortWaitTimeout, false);
}

TEST_F(PluginTest, Flash) {
  TestPlugin(L"flash.html", kShortWaitTimeout, false);
}

TEST_F(PluginTest, FlashOctetStream) {
  TestPlugin(L"flash-octet-stream.html", kShortWaitTimeout, false);
}

TEST_F(PluginTest, FlashSecurity) {
  TestPlugin(L"flash.html", kShortWaitTimeout, false);
}

// http://crbug.com/16114
// Disabled for http://crbug.com/21538
TEST_F(PluginTest, DISABLED_FlashLayoutWhilePainting) {
  TestPlugin(L"flash-layout-while-painting.html", kShortWaitTimeout, true);
}

// http://crbug.com/8690
TEST_F(PluginTest, DISABLED_Java) {
  TestPlugin(L"Java.html", kShortWaitTimeout, false);
}

// Disabled for http://crbug.com/22666
TEST_F(PluginTest, DISABLED_Silverlight) {
  TestPlugin(L"silverlight.html", kShortWaitTimeout, false);
}
