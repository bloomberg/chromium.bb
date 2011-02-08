// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/file_path.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/npapi_test_helper.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

using npapi_test::kTestCompleteCookie;
using npapi_test::kTestCompleteSuccess;

static const FilePath::CharType* kTestDir = FILE_PATH_LITERAL("npapi");

class LayoutPluginTester : public NPAPITesterBase {
 protected:
  LayoutPluginTester() : NPAPITesterBase() {}
};

// Make sure that navigating away from a plugin referenced by JS doesn't
// crash.
TEST_F(LayoutPluginTester, UnloadNoCrash) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("npapi").AppendASCII("layout_test_plugin.html");
  NavigateToURL(net::FilePathToFileURL(path));

  string16 title;
  scoped_refptr<TabProxy> tab = GetActiveTab();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(tab->GetTabTitle(&title));
  EXPECT_EQ(ASCIIToUTF16("Layout Test Plugin Test"), title);

  ASSERT_TRUE(tab->GoBack());
  EXPECT_TRUE(tab->GetTabTitle(&title));
  EXPECT_EQ(string16(), title);
}

// Tests if a plugin executing a self deleting script using NPN_GetURL
// works without crashing or hanging
// Flaky: http://crbug.com/59327
TEST_F(LayoutPluginTester, FLAKY_SelfDeletePluginGetUrl) {
  const FilePath test_case(FILE_PATH_LITERAL("self_delete_plugin_geturl.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("self_delete_plugin_geturl", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
}

// Tests if a plugin executing a self deleting script using Invoke
// works without crashing or hanging
// Flaky. See http://crbug.com/30702
TEST_F(LayoutPluginTester, FLAKY_SelfDeletePluginInvoke) {
  const FilePath test_case(FILE_PATH_LITERAL("self_delete_plugin_invoke.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("self_delete_plugin_invoke", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
}

TEST_F(LayoutPluginTester, NPObjectReleasedOnDestruction) {
  if (ProxyLauncher::in_process_renderer())
    return;

  const FilePath test_case(
      FILE_PATH_LITERAL("npobject_released_on_destruction.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));

  scoped_refptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window_proxy);
  ASSERT_TRUE(window_proxy->AppendTab(GURL(chrome::kAboutBlankURL)));

  scoped_refptr<TabProxy> tab_proxy(window_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->Close(true));
}

// Test that a dialog is properly created when a plugin throws an
// exception.  Should be run for in and out of process plugins, but
// the more interesting case is out of process, where we must route
// the exception to the correct renderer.
TEST_F(LayoutPluginTester, NPObjectSetException) {
  const FilePath test_case(FILE_PATH_LITERAL("npobject_set_exception.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
  ASSERT_NO_FATAL_FAILURE(NavigateToURL(url));
  WaitForFinish("npobject_set_exception", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                TestTimeouts::action_max_timeout_ms());
}
