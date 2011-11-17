// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
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

    // Smooth scrolling confuses the scrollbar test.
    launch_arguments_.AppendSwitch(switches::kDisableSmoothScrolling);
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

    // See comment above TestingInstance in ppapi/test/testing_instance.h.
    // Basically it sets a series of numbered cookies. The value of "..." means
    // it's still working and we should continue to wait, any other value
    // indicates completion (in this case it will start with "PASS" or "FAIL").
    // This keeps us from timing out on cookie waits for long tests.
    int progress_number = 0;
    std::string progress;
    while (true) {
      std::string cookie_name = StringPrintf("PPAPI_PROGRESS_%d",
                                             progress_number);
      progress = WaitUntilCookieNonEmpty(tab.get(), test_url,
            cookie_name.c_str(), TestTimeouts::large_test_timeout_ms());
      if (progress != "...")
        break;
      progress_number++;
    }

    if (progress_number == 0) {
      // Failing the first time probably means the plugin wasn't loaded.
      ASSERT_FALSE(progress.empty())
          << "Plugin couldn't be loaded. Make sure the PPAPI test plugin is "
          << "built, in the right place, and doesn't have any missing symbols.";
    } else {
      ASSERT_FALSE(progress.empty()) << "Test timed out.";
    }

    EXPECT_STREQ("PASS", progress.c_str());
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
TEST_PPAPI_OUT_OF_PROCESS(DISABLED_Instance)

TEST_PPAPI_IN_PROCESS(Graphics2D)
TEST_PPAPI_OUT_OF_PROCESS(Graphics2D)

TEST_PPAPI_IN_PROCESS(ImageData)
TEST_PPAPI_OUT_OF_PROCESS(ImageData)

TEST_PPAPI_IN_PROCESS(Buffer)
TEST_PPAPI_OUT_OF_PROCESS(Buffer)

TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader)

// http://crbug.com/89961
#if defined(OS_WIN)
// It often takes too long time (and fails otherwise) on Windows.
#define MAYBE_URLLoader DISABLED_URLLoader
#else
#define MAYBE_URLLoader FAILS_URLLoader
#endif

TEST_F(OutOfProcessPPAPITest, MAYBE_URLLoader) {
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

TEST_PPAPI_IN_PROCESS(Crypto)
TEST_PPAPI_OUT_OF_PROCESS(Crypto)

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
TEST_PPAPI_IN_PROCESS(PostMessage_SendInInit)
TEST_PPAPI_IN_PROCESS(PostMessage_SendingData)
TEST_PPAPI_IN_PROCESS(PostMessage_MessageEvent)
TEST_PPAPI_IN_PROCESS(PostMessage_NoHandler)
TEST_PPAPI_IN_PROCESS(PostMessage_ExtraParam)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_SendInInit)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_SendingData)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_MessageEvent)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_NoHandler)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_ExtraParam)
#if !defined(OS_WIN)
// Times out on Windows XP: http://crbug.com/95557
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_NonMainThread)
#endif

TEST_PPAPI_IN_PROCESS(Memory)
TEST_PPAPI_OUT_OF_PROCESS(Memory)

TEST_PPAPI_IN_PROCESS(VideoDecoder)
TEST_PPAPI_OUT_OF_PROCESS(VideoDecoder)

// http://crbug.com/90039 and http://crbug.com/83443 (Mac)
TEST_F(PPAPITest, FAILS_FileIO) {
  RunTestViaHTTP("FileIO");
}
// http://crbug.com/101154
TEST_F(OutOfProcessPPAPITest, DISABLED_FileIO) {
  RunTestViaHTTP("FileIO");
}

TEST_PPAPI_IN_PROCESS_VIA_HTTP(FileRef)
// Disabled because it times out: http://crbug.com/89961
//TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(FileRef)

TEST_PPAPI_IN_PROCESS_VIA_HTTP(FileSystem)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(FileSystem)

// http://crbug.com/96767 and 104384 for aura.
#if !defined(OS_MACOSX) && !defined(USE_AURA)
#define MAYBE_FlashFullscreen FLAKY_FlashFullscreen
#define MAYBE_FlashFullscreen FLAKY_FlashFullscreen
#else
#define MAYBE_FlashFullscreen DISABLED_FlashFullscreen
#define MAYBE_FlashFullscreen DISABLED_FlashFullscreen
#endif

TEST_F(PPAPITest, MAYBE_FlashFullscreen) {
  RunTestViaHTTP("FlashFullscreen");
}
TEST_F(OutOfProcessPPAPITest, MAYBE_FlashFullscreen) {
  RunTestViaHTTP("FlashFullscreen");
}
// New implementation only honors fullscreen requests within a context of
// a user gesture. Since we do not yet have an infrastructure for testing
// those under ppapi_tests, the tests below time out when run automtically.
// To test the code, run them manually following the directions here:
//   www.chromium.org/developers/design-documents/pepper-plugin-implementation
// and click on the plugin area (gray square) to force fullscreen mode and
// get the test unstuck.
TEST_F(PPAPITest, DISABLED_Fullscreen) {
  RunTestViaHTTP("Fullscreen");
}
TEST_F(OutOfProcessPPAPITest, DISABLED_Fullscreen) {
  RunTestViaHTTP("Fullscreen");
}

TEST_PPAPI_IN_PROCESS(FlashClipboard)
TEST_PPAPI_OUT_OF_PROCESS(FlashClipboard)

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
// Flaky. http://crbug.com/84294
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

TEST_PPAPI_IN_PROCESS(NetAddressPrivate)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate)

// PPB_TCPSocket_Private currently isn't supported in-process.
TEST_F(OutOfProcessPPAPITest, TCPSocketPrivate) {
  RunTestViaHTTP("TCPSocketPrivate");
}

TEST_PPAPI_IN_PROCESS(Flash_SetInstanceAlwaysOnTop)
TEST_PPAPI_IN_PROCESS(Flash_GetProxyForURL)
TEST_PPAPI_IN_PROCESS(Flash_MessageLoop)
TEST_PPAPI_IN_PROCESS(Flash_GetLocalTimeZoneOffset)
TEST_PPAPI_IN_PROCESS(Flash_GetCommandLineArgs)
TEST_PPAPI_OUT_OF_PROCESS(Flash_SetInstanceAlwaysOnTop)
TEST_PPAPI_OUT_OF_PROCESS(Flash_GetProxyForURL)
TEST_PPAPI_OUT_OF_PROCESS(Flash_MessageLoop)
TEST_PPAPI_OUT_OF_PROCESS(Flash_GetLocalTimeZoneOffset)
TEST_PPAPI_OUT_OF_PROCESS(Flash_GetCommandLineArgs)
