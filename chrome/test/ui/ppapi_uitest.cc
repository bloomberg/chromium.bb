// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"
#include "webkit/plugins/plugin_switches.h"

namespace {

// Platform-specific filename relative to the chrome executable.
#if defined(OS_WIN)
const wchar_t library_name[] = L"ppapi_tests.dll";
#elif defined(OS_MACOSX)
const char library_name[] = "ppapi_tests.plugin";
#elif defined(OS_POSIX)
const char library_name[] = "libppapi_tests.so";
#endif

}  // namespace

class PPAPITest : public UITest {
 public:
  PPAPITest() {
    // Append the switch to register the pepper plugin.
    // library name = <out dir>/<test_name>.<library_extension>
    // MIME type = application/x-ppapi-<test_name>
    FilePath plugin_dir;
    PathService::Get(base::DIR_EXE, &plugin_dir);

    FilePath plugin_lib = plugin_dir.Append(library_name);
    EXPECT_TRUE(file_util::PathExists(plugin_lib));
    FilePath::StringType pepper_plugin = plugin_lib.value();
    pepper_plugin.append(FILE_PATH_LITERAL(";application/x-ppapi-tests"));
    launch_arguments_.AppendSwitchNative(switches::kRegisterPepperPlugins,
                                         pepper_plugin);

    // The test sends us the result via a cookie.
    launch_arguments_.AppendSwitch(switches::kEnableFileCookies);

    // Some stuff is hung off of the testing interface which is not enabled
    // by default.
    launch_arguments_.AppendSwitch(switches::kEnablePepperTesting);

    // Give unlimited quota for files to Pepper tests.
    // TODO(dumi): remove this switch once we have a quota management
    // system in place.
    launch_arguments_.AppendSwitch(switches::kUnlimitedQuotaForFiles);
  }

  void RunTest(const std::string& test_case) {
    FilePath test_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &test_path);
    test_path = test_path.Append(FILE_PATH_LITERAL("ppapi"));
    test_path = test_path.Append(FILE_PATH_LITERAL("tests"));
    test_path = test_path.Append(FILE_PATH_LITERAL("test_case.html"));

    // Sanity check the file name.
    EXPECT_TRUE(file_util::PathExists(test_path));

    GURL::Replacements replacements;
    replacements.SetQuery(test_case.c_str(),
                          url_parse::Component(0, test_case.size()));
    GURL test_url = net::FilePathToFileURL(test_path);
    RunTestURL(test_url.ReplaceComponents(replacements));
  }

  void RunTestViaHTTP(const std::string& test_case) {
    net::TestServer test_server(
        net::TestServer::TYPE_HTTP,
        FilePath(FILE_PATH_LITERAL("ppapi/tests")));
    ASSERT_TRUE(test_server.Start());
    RunTestURL(test_server.GetURL("files/test_case.html?" + test_case));
  }

 private:
  void RunTestURL(const GURL& test_url) {
    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());
    ASSERT_TRUE(tab->NavigateToURL(test_url));

    // First wait for the "starting" signal. This cookie is set at the start
    // of every test. Waiting for this separately allows us to avoid a single
    // long timeout. Instead, we can have two timeouts which allow startup +
    // test execution time to take a while on a loaded computer, while also
    // making sure we're making forward progress.
    std::string startup_cookie =
        WaitUntilCookieNonEmpty(tab.get(), test_url,
            "STARTUP_COOKIE", TestTimeouts::action_max_timeout_ms());

    // If this fails, the plugin couldn't be loaded in the given amount of
    // time. This may mean the plugin was not found or possibly the system
    // can't load it due to missing symbols, etc.
    ASSERT_STREQ("STARTED", startup_cookie.c_str())
        << "Plugin couldn't be loaded. Make sure the PPAPI test plugin is "
        << "built, in the right place, and doesn't have any missing symbols.";

    std::string escaped_value =
        WaitUntilCookieNonEmpty(tab.get(), test_url,
            "COMPLETION_COOKIE", TestTimeouts::large_test_timeout_ms());
    EXPECT_STREQ("PASS", escaped_value.c_str());
  }
};

TEST_F(PPAPITest, FAILS_Instance) {
  RunTest("Instance");
}

TEST_F(PPAPITest, Graphics2D) {
  RunTest("Graphics2D");
}

// TODO(brettw) bug 51344: this is flaky on bots. Seems to timeout navigating.
// Possibly all the image allocations slow things down on a loaded bot too much.
TEST_F(PPAPITest, FLAKY_ImageData) {
  RunTest("ImageData");
}

TEST_F(PPAPITest, Buffer) {
  RunTest("Buffer");
}

// Flakey, http:L//crbug.com/57053
TEST_F(PPAPITest, FLAKY_URLLoader) {
  RunTestViaHTTP("URLLoader");
}

// Flaky, http://crbug.com/51012
TEST_F(PPAPITest, FLAKY_PaintAggregator) {
  RunTestViaHTTP("PaintAggregator");
}

// Flaky, http://crbug.com/48544.
TEST_F(PPAPITest, FLAKY_Scrollbar) {
  RunTest("Scrollbar");
}

TEST_F(PPAPITest, UrlUtil) {
  RunTest("UrlUtil");
}

TEST_F(PPAPITest, CharSet) {
  RunTest("CharSet");
}

TEST_F(PPAPITest, Var) {
  RunTest("Var");
}

TEST_F(PPAPITest, FileIO) {
  RunTestViaHTTP("FileIO");
}

TEST_F(PPAPITest, FileRef) {
  RunTestViaHTTP("FileRef");
}

// http://crbug.com/63239
TEST_F(PPAPITest, DISABLED_DirectoryReader) {
  RunTestViaHTTP("DirectoryReader");
}
