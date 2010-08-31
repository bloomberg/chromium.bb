// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the top plugins to catch regressions in our plugin host code, as
// well as in the out of process code.  Currently this tests:
//  Flash
//  Real
//  QuickTime
//  Windows Media Player
//    -this includes both WMP plugins.  npdsplay.dll is the older one that
//     comes with XP.  np-mswmp.dll can be downloaded from Microsoft and
//     needs SP2 or Vista.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <comutil.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "third_party/npapi/bindings/npapi.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_list.h"

#if defined(OS_WIN)
#include "base/registry.h"
#endif

class PluginTest : public UITest {
 protected:
#if defined(OS_WIN)
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
      launch_arguments_.AppendSwitchASCII(switches::kTestSandbox,
                                          "security_tests.dll");
    }

    UITest::SetUp();
  }
#endif  // defined(OS_WIN)

  void TestPlugin(const std::string& test_case,
                  int timeout,
                  bool mock_http) {
    GURL url = GetTestUrl(test_case, mock_http);
    NavigateToURL(url);
    WaitForFinish(timeout, mock_http);
  }

  // Generate the URL for testing a particular test.
  // HTML for the tests is all located in test_directory\plugin\<testcase>
  // Set |mock_http| to true to use mock HTTP server.
  GURL GetTestUrl(const std::string &test_case, bool mock_http) {
    static const FilePath::CharType kPluginPath[] = FILE_PATH_LITERAL("plugin");
    if (mock_http) {
      FilePath plugin_path = FilePath(kPluginPath).AppendASCII(test_case);
      return URLRequestMockHTTPJob::GetMockUrl(plugin_path);
    }

    FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.Append(kPluginPath).AppendASCII(test_case);
    return net::FilePathToFileURL(path);
  }

  // Waits for the test case to finish.
  void WaitForFinish(const int wait_time, bool mock_http) {
    static const char kTestCompleteCookie[] = "status";
    static const char kTestCompleteSuccess[] = "OK";

    GURL url = GetTestUrl("done", mock_http);
    scoped_refptr<TabProxy> tab(GetActiveTab());

    const std::string result =
        WaitUntilCookieNonEmpty(tab, url, kTestCompleteCookie, wait_time);
    ASSERT_EQ(kTestCompleteSuccess, result);
  }
};

TEST_F(PluginTest, Flash) {
  // Note: This does not work with the npwrapper on 64-bit Linux. Install the
  // native 64-bit Flash to run the test.
  // TODO(thestig) Update this list if we decide to only test against internal
  // Flash plugin in the future?
  std::string kFlashQuery =
#if defined(OS_WIN)
      "npswf32.dll"
#elif defined(OS_MACOSX)
      "Flash Player.plugin"
#elif defined(OS_POSIX)
      "libflashplayer.so"
#endif
      ;
  TestPlugin("flash.html?" + kFlashQuery, action_max_timeout_ms(), false);
}

#if defined(OS_WIN)
// Windows only test
TEST_F(PluginTest, FlashSecurity) {
  TestPlugin("flash.html", action_max_timeout_ms(), false);
}
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
// TODO(port) Port the following tests to platforms that have the required
// plugins.
TEST_F(PluginTest, Quicktime) {
  TestPlugin("quicktime.html", action_max_timeout_ms(), false);
}

// Disabled on Release bots - http://crbug.com/44662
#if defined(NDEBUG)
#define MediaPlayerNew DISABLED_MediaPlayerNew
#endif
TEST_F(PluginTest, MediaPlayerNew) {
  TestPlugin("wmp_new.html", action_max_timeout_ms(), false);
}

// http://crbug.com/4809
TEST_F(PluginTest, DISABLED_MediaPlayerOld) {
  TestPlugin("wmp_old.html", action_max_timeout_ms(), false);
}

#if defined(NDEBUG)
#define Real DISABLED_Real
#endif
// Disabled on Release bots - http://crbug.com/44673
TEST_F(PluginTest, Real) {
  TestPlugin("real.html", action_max_timeout_ms(), false);
}

TEST_F(PluginTest, FlashOctetStream) {
  TestPlugin("flash-octet-stream.html", action_max_timeout_ms(), false);
}

#if defined(OS_WIN)
// http://crbug.com/53926
TEST_F(PluginTest, FLAKY_FlashLayoutWhilePainting) {
#else
TEST_F(PluginTest, FlashLayoutWhilePainting) {
#endif
  TestPlugin("flash-layout-while-painting.html", action_max_timeout_ms(), true);
}

// http://crbug.com/8690
TEST_F(PluginTest, DISABLED_Java) {
  TestPlugin("Java.html", action_max_timeout_ms(), false);
}

TEST_F(PluginTest, Silverlight) {
  TestPlugin("silverlight.html", action_max_timeout_ms(), false);
}
#endif  // defined(OS_WIN)
