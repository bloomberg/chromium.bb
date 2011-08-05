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

// In-process plugin test runner.  See OutOfProcessPPAPITest below for the
// out-of-process version.
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
    std::string query("testcase=");
    query += test_case;
    replacements.SetQuery(query.c_str(), url_parse::Component(0, query.size()));
    GURL test_url = net::FilePathToFileURL(test_path);
    RunTestURL(test_url.ReplaceComponents(replacements));
  }

  void RunTestViaHTTP(const std::string& test_case) {
    net::TestServer test_server(
        net::TestServer::TYPE_HTTP,
        FilePath(FILE_PATH_LITERAL("ppapi/tests")));
    ASSERT_TRUE(test_server.Start());
    RunTestURL(
        test_server.GetURL("files/test_case.html?testcase=" + test_case));
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

// Variant of PPAPITest that runs plugins out-of-process to test proxy
// codepaths.
class OutOfProcessPPAPITest : public PPAPITest {
 public:
  OutOfProcessPPAPITest() {
    // Run PPAPI out-of-process to exercise proxy implementations.
    launch_arguments_.AppendSwitch(switches::kPpapiOutOfProcess);
  }
};

// Use these macros to run the tests for a specific interface.
// Most interfaces should be tested with both macros.
#define TEST_PPAPI_IN_PROCESS(test_name) \
    TEST_F(PPAPITest, test_name) { \
      RunTest(#test_name); \
    }
#define TEST_PPAPI_OUT_OF_PROCESS(test_name) \
    TEST_F(OutOfProcessPPAPITest, test_name) { \
      RunTest(#test_name); \
    }

// Similar macros that test over HTTP.
#define TEST_PPAPI_IN_PROCESS_VIA_HTTP(test_name) \
    TEST_F(PPAPITest, test_name) { \
      RunTestViaHTTP(#test_name); \
    }
#define TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(test_name) \
    TEST_F(OutOfProcessPPAPITest, test_name) { \
      RunTestViaHTTP(#test_name); \
    }


//
// Interface tests.
//

TEST_PPAPI_IN_PROCESS(Broker)
TEST_PPAPI_OUT_OF_PROCESS(Broker)

TEST_PPAPI_IN_PROCESS(Core)
TEST_PPAPI_OUT_OF_PROCESS(Core)

TEST_PPAPI_IN_PROCESS(CursorControl)
TEST_PPAPI_OUT_OF_PROCESS(CursorControl)

TEST_PPAPI_IN_PROCESS(Instance)

// http://crbug.com/91729
#if defined(OS_LINUX)
#define MAYBE_Instance FLAKY_Instance
#else
#define MAYBE_Instance Instance
#endif

TEST_PPAPI_OUT_OF_PROCESS(MAYBE_Instance)

TEST_PPAPI_IN_PROCESS(Graphics2D)
// Disabled because it times out: http://crbug.com/89961
//TEST_PPAPI_OUT_OF_PROCESS(Graphics2D)

TEST_PPAPI_IN_PROCESS(ImageData)
TEST_PPAPI_OUT_OF_PROCESS(ImageData)

TEST_PPAPI_IN_PROCESS(Buffer)
TEST_PPAPI_OUT_OF_PROCESS(Buffer)

TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader)
// http://crbug.com/89961
TEST_F(OutOfProcessPPAPITest, FAILS_URLLoader) {
  RunTestViaHTTP("URLLoader");
}

TEST_PPAPI_IN_PROCESS(PaintAggregator)
TEST_PPAPI_OUT_OF_PROCESS(PaintAggregator)

TEST_PPAPI_IN_PROCESS(Scrollbar)
// http://crbug.com/89961
TEST_F(OutOfProcessPPAPITest, FAILS_Scrollbar) {
  RunTest("Scrollbar");
}

TEST_PPAPI_IN_PROCESS(URLUtil)
TEST_PPAPI_OUT_OF_PROCESS(URLUtil)

TEST_PPAPI_IN_PROCESS(CharSet)
TEST_PPAPI_OUT_OF_PROCESS(CharSet)

TEST_PPAPI_IN_PROCESS(Var)
// http://crbug.com/89961
TEST_F(OutOfProcessPPAPITest, FAILS_Var) {
  RunTest("Var");
}

TEST_PPAPI_IN_PROCESS(VarDeprecated)
// Disabled because it times out: http://crbug.com/89961
//TEST_PPAPI_OUT_OF_PROCESS(VarDeprecated)

// Windows defines 'PostMessage', so we have to undef it.
#ifdef PostMessage
#undef PostMessage
#endif
TEST_PPAPI_IN_PROCESS(PostMessage)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage)

TEST_PPAPI_IN_PROCESS(Memory)
TEST_PPAPI_OUT_OF_PROCESS(Memory)

TEST_PPAPI_IN_PROCESS(QueryPolicy)
//TEST_PPAPI_OUT_OF_PROCESS(QueryPolicy)

// http://crbug.com/90039 and http://crbug.com/83443 (Mac)
TEST_F(PPAPITest, FAILS_FileIO) {
  RunTestViaHTTP("FileIO");
}
TEST_F(OutOfProcessPPAPITest, FAILS_FileIO) {
  RunTestViaHTTP("FileIO");
}

TEST_PPAPI_IN_PROCESS_VIA_HTTP(FileRef)
// Disabled because it times out: http://crbug.com/89961
//TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(FileRef)

TEST_PPAPI_IN_PROCESS_VIA_HTTP(FileSystem)
// http://crbug.com/90040
TEST_F(OutOfProcessPPAPITest, FLAKY_FileSystem) {
  RunTestViaHTTP("FileSystem");
}

#if defined(OS_POSIX)
#define MAYBE_DirectoryReader FLAKY_DirectoryReader
#else
#define MAYBE_DirectoryReader DirectoryReader
#endif

// Flaky on Mac + Linux, maybe http://codereview.chromium.org/7094008
TEST_F(PPAPITest, MAYBE_DirectoryReader) {
  RunTestViaHTTP("DirectoryReader");
}
// http://crbug.com/89961
TEST_F(OutOfProcessPPAPITest, FAILS_DirectoryReader) {
  RunTestViaHTTP("DirectoryReader");
}

#if defined(ENABLE_P2P_APIS)
// Flaky. http://crbug.com/84295
TEST_F(PPAPITest, FLAKY_Transport) {
  RunTest("Transport");
}
// http://crbug.com/89961
TEST_F(OutOfProcessPPAPITest, FAILS_Transport) {
  RunTestViaHTTP("Transport");
}
#endif // ENABLE_P2P_APIS

TEST_PPAPI_IN_PROCESS(UMA)
// There is no proxy.
TEST_F(OutOfProcessPPAPITest, FAILS_UMA) {
  RunTest("UMA");
}

TEST_PPAPI_IN_PROCESS(VideoDecoder)
// There is no proxy yet (vrk is adding it).
TEST_F(OutOfProcessPPAPITest, FAILS_VideoDecoder) {
  RunTest("VideoDecoder");
}
