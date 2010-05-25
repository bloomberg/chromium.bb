// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/npapi_test_helper.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
static const char kTestPluginName[] = "npapi_layout_test_plugin.dll";
#elif defined(OS_MACOSX)
static const char kTestPluginName[] = "TestNetscapePlugIn.plugin";
#elif defined(OS_LINUX)
static const char kTestPluginName[] = "libnpapi_layout_test_plugin.so";
#endif

namespace {

class LayoutPluginTester : public NPAPITesterBase {
 protected:
  LayoutPluginTester() : NPAPITesterBase(kTestPluginName) {}
};

}  // namespace

// Make sure that navigating away from a plugin referenced by JS doesn't
// crash.
TEST_F(LayoutPluginTester, UnloadNoCrash) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("npapi").AppendASCII("layout_test_plugin.html");
  NavigateToURL(net::FilePathToFileURL(path));

  std::wstring title;
  scoped_refptr<TabProxy> tab = GetActiveTab();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(tab->GetTabTitle(&title));
  EXPECT_EQ(L"Layout Test Plugin Test", title);

  ASSERT_TRUE(tab->GoBack());
  EXPECT_TRUE(tab->GetTabTitle(&title));
  EXPECT_EQ(L"", title);
}
