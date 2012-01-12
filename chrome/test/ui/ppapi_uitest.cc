// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/common/pepper_plugin_registry.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "content/public/common/url_constants.h"
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

class PPAPITestBase : public UITest {
 public:
  PPAPITestBase() {
    // The test sends us the result via a cookie.
    launch_arguments_.AppendSwitch(switches::kEnableFileCookies);

    // Some stuff is hung off of the testing interface which is not enabled
    // by default.
    launch_arguments_.AppendSwitch(switches::kEnablePepperTesting);

    // Smooth scrolling confuses the scrollbar test.
    launch_arguments_.AppendSwitch(switches::kDisableSmoothScrolling);
  }

  virtual std::string BuildQuery(const std::string& base,
                                 const std::string& test_case) = 0;

  // Returns the URL to load for file: tests.
  GURL GetTestFileUrl(const std::string& test_case) {
    FilePath test_path;
    EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &test_path));
    test_path = test_path.Append(FILE_PATH_LITERAL("ppapi"));
    test_path = test_path.Append(FILE_PATH_LITERAL("tests"));
    test_path = test_path.Append(FILE_PATH_LITERAL("test_case.html"));

    // Sanity check the file name.
    EXPECT_TRUE(file_util::PathExists(test_path));

    GURL test_url = net::FilePathToFileURL(test_path);

    GURL::Replacements replacements;
    std::string query = BuildQuery("", test_case);
    replacements.SetQuery(query.c_str(), url_parse::Component(0, query.size()));
    return test_url.ReplaceComponents(replacements);
  }

  void RunTest(const std::string& test_case) {
    scoped_refptr<TabProxy> tab = GetActiveTab();
    EXPECT_TRUE(tab.get());
    if (!tab.get())
      return;
    GURL url = GetTestFileUrl(test_case);
    EXPECT_TRUE(tab->NavigateToURLBlockUntilNavigationsComplete(url, 1));
    RunTestURL(tab, url);
  }

  void RunTestViaHTTP(const std::string& test_case) {
    // For HTTP tests, we use the output DIR to grab the generated files such
    // as the NEXEs.
    FilePath exe_dir = CommandLine::ForCurrentProcess()->GetProgram().DirName();
    FilePath src_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));

    // TestServer expects a path relative to source. So we must first
    // generate absolute paths to SRC and EXE and from there generate
    // a relative path.
    if (!exe_dir.IsAbsolute()) file_util::AbsolutePath(&exe_dir);
    if (!src_dir.IsAbsolute()) file_util::AbsolutePath(&src_dir);
    ASSERT_TRUE(exe_dir.IsAbsolute());
    ASSERT_TRUE(src_dir.IsAbsolute());

    size_t match, exe_size, src_size;
    std::vector<FilePath::StringType> src_parts, exe_parts;

    // Determine point at which src and exe diverge, and create a relative path.
    exe_dir.GetComponents(&exe_parts);
    src_dir.GetComponents(&src_parts);
    exe_size = exe_parts.size();
    src_size = src_parts.size();
    for (match = 0; match < exe_size && match < src_size; ++match) {
      if (exe_parts[match] != src_parts[match])
        break;
    }
    FilePath web_dir;
    for (size_t tmp_itr = match; tmp_itr < src_size; ++tmp_itr) {
      web_dir = web_dir.Append(FILE_PATH_LITERAL(".."));
    }
    for (; match < exe_size; ++match) {
      web_dir = web_dir.Append(exe_parts[match]);
    }

    net::TestServer test_server(net::TestServer::TYPE_HTTP, web_dir);
    ASSERT_TRUE(test_server.Start());
    std::string query = BuildQuery("files/test_case.html?", test_case);

    scoped_refptr<TabProxy> tab = GetActiveTab();
    GURL url = test_server.GetURL(query);
    EXPECT_TRUE(tab->NavigateToURLBlockUntilNavigationsComplete(url, 1));
    RunTestURL(tab, url);
  }

  void RunTestWithWebSocketServer(const std::string& test_case) {
    FilePath websocket_root_dir;
    ASSERT_TRUE(
        PathService::Get(chrome::DIR_LAYOUT_TESTS, &websocket_root_dir));
    // TODO(toyoshim): Remove following logging after a bug investigation.
    // http://crbug.com/107836 .
    LOG(INFO) << "Assume LayoutTests in " << websocket_root_dir.MaybeAsASCII();

    ui_test_utils::TestWebSocketServer server;
    ASSERT_TRUE(server.Start(websocket_root_dir));
    RunTest(test_case);
  }

  std::string StripPrefixes(const std::string& test_name) {
    const char* const prefixes[] = { "FAILS_", "FLAKY_", "DISABLED_" };
    for (size_t i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); ++i)
      if (test_name.find(prefixes[i]) == 0)
        return test_name.substr(strlen(prefixes[i]));
    return test_name;
  }

 protected:
  // Runs the test for a tab given the tab that's already navigated to the
  // given URL.
  void RunTestURL(scoped_refptr<TabProxy> tab, const GURL& test_url) {
    ASSERT_TRUE(tab.get());

    // The large timeout was causing the cycle time for the whole test suite
    // to be too long when a tiny bug caused all tests to timeout.
    // http://crbug.com/108264
    int timeout_ms = 90000;
    //int timeout_ms = TestTimeouts::large_test_timeout_ms());

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
                                         cookie_name.c_str(), timeout_ms);
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

// In-process plugin test runner.  See OutOfProcessPPAPITest below for the
// out-of-process version.
class PPAPITest : public PPAPITestBase {
 public:
  PPAPITest() {
    // Append the switch to register the pepper plugin.
    // library name = <out dir>/<test_name>.<library_extension>
    // MIME type = application/x-ppapi-<test_name>
    FilePath plugin_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_EXE, &plugin_dir));

    FilePath plugin_lib = plugin_dir.Append(library_name);
    EXPECT_TRUE(file_util::PathExists(plugin_lib));
    FilePath::StringType pepper_plugin = plugin_lib.value();
    pepper_plugin.append(FILE_PATH_LITERAL(";application/x-ppapi-tests"));
    launch_arguments_.AppendSwitchNative(switches::kRegisterPepperPlugins,
                                         pepper_plugin);
    launch_arguments_.AppendSwitchASCII(switches::kAllowNaClSocketAPI,
                                        "127.0.0.1");
  }

  std::string BuildQuery(const std::string& base,
                         const std::string& test_case){
    return StringPrintf("%stestcase=%s", base.c_str(), test_case.c_str());
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

// NaCl plugin test runner.
class PPAPINaClTest : public PPAPITestBase {
 public:
  PPAPINaClTest() {
    FilePath plugin_lib;
    EXPECT_TRUE(PathService::Get(chrome::FILE_NACL_PLUGIN, &plugin_lib));
    EXPECT_TRUE(file_util::PathExists(plugin_lib));

    // Enable running NaCl outside of the store.
    launch_arguments_.AppendSwitch(switches::kEnableNaCl);
    launch_arguments_.AppendSwitchASCII(switches::kAllowNaClSocketAPI,
                                        "127.0.0.1");
  }

  // Append the correct mode and testcase string
  std::string BuildQuery(const std::string& base,
                         const std::string& test_case) {
    return StringPrintf("%smode=nacl&testcase=%s", base.c_str(),
                        test_case.c_str());
  }
};

class PPAPINaClTestDisallowedSockets : public PPAPITestBase {
 public:
  PPAPINaClTestDisallowedSockets() {
    FilePath plugin_lib;
    EXPECT_TRUE(PathService::Get(chrome::FILE_NACL_PLUGIN, &plugin_lib));
    EXPECT_TRUE(file_util::PathExists(plugin_lib));

    // Enable running NaCl outside of the store.
    launch_arguments_.AppendSwitch(switches::kEnableNaCl);
  }

  // Append the correct mode and testcase string
  std::string BuildQuery(const std::string& base,
                         const std::string& test_case) {
    return StringPrintf("%smode=nacl&testcase=%s", base.c_str(),
                        test_case.c_str());
  }
};

// This macro finesses macro expansion to do what we want.
#define STRIP_PREFIXES(test_name) StripPrefixes(#test_name)

// Use these macros to run the tests for a specific interface.
// Most interfaces should be tested with both macros.
#define TEST_PPAPI_IN_PROCESS(test_name) \
    TEST_F(PPAPITest, test_name) { \
      RunTest(STRIP_PREFIXES(test_name)); \
    }
#define TEST_PPAPI_OUT_OF_PROCESS(test_name) \
    TEST_F(OutOfProcessPPAPITest, test_name) { \
      RunTest(STRIP_PREFIXES(test_name)); \
    }

// Similar macros that test over HTTP.
#define TEST_PPAPI_IN_PROCESS_VIA_HTTP(test_name) \
    TEST_F(PPAPITest, test_name) { \
      RunTestViaHTTP(STRIP_PREFIXES(test_name)); \
    }
#define TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(test_name) \
    TEST_F(OutOfProcessPPAPITest, test_name) { \
      RunTestViaHTTP(STRIP_PREFIXES(test_name)); \
    }

// Similar macros that test with WebSocket server
#define TEST_PPAPI_IN_PROCESS_WITH_WS(test_name) \
    TEST_F(PPAPITest, test_name) { \
      RunTestWithWebSocketServer(STRIP_PREFIXES(test_name)); \
    }
#define TEST_PPAPI_OUT_OF_PROCESS_WITH_WS(test_name) \
    TEST_F(OutOfProcessPPAPITest, test_name) { \
      RunTestWithWebSocketServer(STRIP_PREFIXES(test_name)); \
    }


#if defined(DISABLE_NACL)
#define TEST_PPAPI_NACL_VIA_HTTP(test_name)
#define TEST_PPAPI_NACL_VIA_HTTP_DISALLOWED_SOCKETS(test_name)
#else

// NaCl based PPAPI tests
#define TEST_PPAPI_NACL_VIA_HTTP(test_name) \
    TEST_F(PPAPINaClTest, test_name) { \
  RunTestViaHTTP(STRIP_PREFIXES(test_name)); \
}

// NaCl based PPAPI tests with disallowed socket API
#define TEST_PPAPI_NACL_VIA_HTTP_DISALLOWED_SOCKETS(test_name) \
    TEST_F(PPAPINaClTestDisallowedSockets, test_name) { \
  RunTestViaHTTP(STRIP_PREFIXES(test_name)); \
}
#endif


//
// Interface tests.
//

// Disable tests under ASAN.  http://crbug.com/104832.
// This is a bit heavy handed, but the majority of these tests fail under ASAN.
// See bug for history.
#if !defined(ADDRESS_SANITIZER)

TEST_PPAPI_IN_PROCESS(Broker)
TEST_PPAPI_OUT_OF_PROCESS(Broker)

TEST_PPAPI_IN_PROCESS(Core)
TEST_PPAPI_OUT_OF_PROCESS(Core)

// Times out on Linux. http://crbug.com/108180
#if defined(OS_LINUX)
#define MAYBE_CursorControl DISABLED_CursorControl
#else
#define MAYBE_CursorControl CursorControl
#endif

TEST_PPAPI_IN_PROCESS(MAYBE_CursorControl)
TEST_PPAPI_OUT_OF_PROCESS(MAYBE_CursorControl)
TEST_PPAPI_NACL_VIA_HTTP(MAYBE_CursorControl)

// Times out on Linux. http://crbug.com/108859
#if defined(OS_LINUX)
#define MAYBE_InputEvent DISABLED_InputEvent
#elif defined(OS_MACOSX)
// Flaky on Mac. http://crbug.com/109258
#define MAYBE_InputEvent FLAKY_InputEvent
#else
#define MAYBE_InputEvent InputEvent
#endif

TEST_PPAPI_IN_PROCESS(MAYBE_InputEvent)
TEST_PPAPI_OUT_OF_PROCESS(MAYBE_InputEvent)
// TODO(bbudge) Enable when input events are proxied correctly for NaCl.
TEST_PPAPI_NACL_VIA_HTTP(DISABLED_InputEvent)

TEST_PPAPI_IN_PROCESS(Instance)
TEST_PPAPI_OUT_OF_PROCESS(Instance)
// The Instance test is actually InstanceDeprecated which isn't supported
// by NaCl.

TEST_PPAPI_IN_PROCESS(Graphics2D)
TEST_PPAPI_OUT_OF_PROCESS(Graphics2D)
TEST_PPAPI_NACL_VIA_HTTP(Graphics2D)

TEST_PPAPI_IN_PROCESS(ImageData)
TEST_PPAPI_OUT_OF_PROCESS(ImageData)
TEST_PPAPI_NACL_VIA_HTTP(ImageData)

TEST_PPAPI_IN_PROCESS(Buffer)
TEST_PPAPI_OUT_OF_PROCESS(Buffer)

// TODO(ygorshenin): investigate why
// TEST_PPAPI_IN_PROCESS(TCPSocketPrivateShared) fails,
// http://crbug.com/105860.
TEST_PPAPI_IN_PROCESS_VIA_HTTP(TCPSocketPrivateShared)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(TCPSocketPrivateShared)
TEST_PPAPI_NACL_VIA_HTTP(TCPSocketPrivateShared)

// TODO(ygorshenin): investigate why
// TEST_PPAPI_IN_PROCESS(UDPSocketPrivateShared) fails,
// http://crbug.com/105860.
TEST_PPAPI_IN_PROCESS_VIA_HTTP(UDPSocketPrivateShared)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(UDPSocketPrivateShared)
TEST_PPAPI_NACL_VIA_HTTP(UDPSocketPrivateShared)

TEST_PPAPI_NACL_VIA_HTTP_DISALLOWED_SOCKETS(TCPSocketPrivateDisallowed)
TEST_PPAPI_NACL_VIA_HTTP_DISALLOWED_SOCKETS(UDPSocketPrivateDisallowed)

// URLLoader tests.
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_BasicGET)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_BasicPOST)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_BasicFilePOST)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_BasicFileRangePOST)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_CompoundBodyPOST)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_EmptyDataPOST)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_BinaryDataPOST)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_CustomRequestHeader)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_FailsBogusContentLength)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_StreamToFile)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_UntrustedSameOriginRestriction)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_TrustedSameOriginRestriction)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_UntrustedCrossOriginRequest)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_TrustedCrossOriginRequest)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_UntrustedJavascriptURLRestriction)
// TODO(bbudge) Fix Javascript URLs for trusted loaders.
// http://crbug.com/103062
TEST_PPAPI_IN_PROCESS_VIA_HTTP(FAILS_URLLoader_TrustedJavascriptURLRestriction)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_UntrustedHttpRestriction)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_TrustedHttpRestriction)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_FollowURLRedirect)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_AuditURLRedirect)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_AbortCalls)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_UntendedLoad)

TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_BasicGET)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_BasicPOST)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_BasicFilePOST)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_BasicFileRangePOST)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_CompoundBodyPOST)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_EmptyDataPOST)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_BinaryDataPOST)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_CustomRequestHeader)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_FailsBogusContentLength)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_StreamToFile)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_UntrustedSameOriginRestriction)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_TrustedSameOriginRestriction)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_UntrustedCrossOriginRequest)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_TrustedCrossOriginRequest)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_UntrustedJavascriptURLRestriction)
// TODO(bbudge) Fix Javascript URLs for trusted loaders.
// http://crbug.com/103062
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(
    FAILS_URLLoader_TrustedJavascriptURLRestriction)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_UntrustedHttpRestriction)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_TrustedHttpRestriction)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_FollowURLRedirect)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_AuditURLRedirect)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_AbortCalls)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLLoader_UntendedLoad)

TEST_PPAPI_NACL_VIA_HTTP(URLLoader_BasicGET)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_BasicPOST)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_BasicFilePOST)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_BasicFileRangePOST)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_CompoundBodyPOST)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_EmptyDataPOST)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_BinaryDataPOST)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_CustomRequestHeader)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_FailsBogusContentLength)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_StreamToFile)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_UntrustedSameOriginRestriction)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_UntrustedCrossOriginRequest)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_UntrustedJavascriptURLRestriction)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_UntrustedHttpRestriction)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_FollowURLRedirect)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_AuditURLRedirect)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_AbortCalls)
TEST_PPAPI_NACL_VIA_HTTP(URLLoader_UntendedLoad)

TEST_PPAPI_IN_PROCESS(PaintAggregator)
TEST_PPAPI_OUT_OF_PROCESS(PaintAggregator)
TEST_PPAPI_NACL_VIA_HTTP(PaintAggregator)

TEST_PPAPI_IN_PROCESS(Scrollbar)
// http://crbug.com/89961
TEST_F(OutOfProcessPPAPITest, FAILS_Scrollbar) {
  RunTest("Scrollbar");
}
TEST_PPAPI_NACL_VIA_HTTP(Scrollbar)

TEST_PPAPI_IN_PROCESS(URLUtil)
TEST_PPAPI_OUT_OF_PROCESS(URLUtil)

TEST_PPAPI_IN_PROCESS(CharSet)
TEST_PPAPI_OUT_OF_PROCESS(CharSet)

TEST_PPAPI_IN_PROCESS(Crypto)
TEST_PPAPI_OUT_OF_PROCESS(Crypto)

TEST_PPAPI_IN_PROCESS(Var)
TEST_PPAPI_OUT_OF_PROCESS(Var)
TEST_PPAPI_NACL_VIA_HTTP(Var)

TEST_PPAPI_IN_PROCESS(VarDeprecated)
TEST_PPAPI_OUT_OF_PROCESS(VarDeprecated)

// Windows defines 'PostMessage', so we have to undef it.
#ifdef PostMessage
#undef PostMessage
#endif
TEST_PPAPI_IN_PROCESS(PostMessage_SendInInit)
TEST_PPAPI_IN_PROCESS(PostMessage_SendingData)
TEST_PPAPI_IN_PROCESS(PostMessage_SendingArrayBuffer)
TEST_PPAPI_IN_PROCESS(PostMessage_MessageEvent)
TEST_PPAPI_IN_PROCESS(PostMessage_NoHandler)
TEST_PPAPI_IN_PROCESS(PostMessage_ExtraParam)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_SendInInit)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_SendingData)
TEST_PPAPI_OUT_OF_PROCESS(DISABLED_PostMessage_SendingArrayBuffer)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_MessageEvent)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_NoHandler)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_ExtraParam)
#if !defined(OS_WIN)
// Times out on Windows XP: http://crbug.com/95557
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_NonMainThread)
#endif
TEST_PPAPI_NACL_VIA_HTTP(PostMessage_SendInInit)
TEST_PPAPI_NACL_VIA_HTTP(PostMessage_SendingData)
TEST_PPAPI_NACL_VIA_HTTP(PostMessage_SendingArrayBuffer)
TEST_PPAPI_NACL_VIA_HTTP(PostMessage_MessageEvent)
TEST_PPAPI_NACL_VIA_HTTP(PostMessage_NoHandler)
TEST_PPAPI_NACL_VIA_HTTP(PostMessage_ExtraParam)

TEST_PPAPI_IN_PROCESS(Memory)
TEST_PPAPI_OUT_OF_PROCESS(Memory)
TEST_PPAPI_NACL_VIA_HTTP(Memory)

TEST_PPAPI_IN_PROCESS(VideoDecoder)
TEST_PPAPI_OUT_OF_PROCESS(VideoDecoder)

// Touch and SetLength fail on Mac and Linux due to sandbox restrictions.
// http://crbug.com/101128
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_FileIO_ReadWriteSetLength DISABLED_FileIO_ReadWriteSetLength
#define MAYBE_FileIO_TouchQuery DISABLED_FileIO_TouchQuery
#define MAYBE_FileIO_WillWriteWillSetLength \
    DISABLED_FileIO_WillWriteWillSetLength
#else
#define MAYBE_FileIO_ReadWriteSetLength FileIO_ReadWriteSetLength
#define MAYBE_FileIO_TouchQuery FileIO_TouchQuery
#define MAYBE_FileIO_WillWriteWillSetLength FileIO_WillWriteWillSetLength
#endif

TEST_PPAPI_IN_PROCESS_VIA_HTTP(FileIO_Open)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(FileIO_AbortCalls)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(FileIO_ParallelReads)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(FileIO_ParallelWrites)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(FileIO_NotAllowMixedReadWrite)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(MAYBE_FileIO_ReadWriteSetLength)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(MAYBE_FileIO_TouchQuery)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(MAYBE_FileIO_WillWriteWillSetLength)

TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(FileIO_Open)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(FileIO_AbortCalls)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(FileIO_ParallelReads)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(FileIO_ParallelWrites)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(FileIO_NotAllowMixedReadWrite)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(MAYBE_FileIO_ReadWriteSetLength)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(MAYBE_FileIO_TouchQuery)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(MAYBE_FileIO_WillWriteWillSetLength)

TEST_PPAPI_NACL_VIA_HTTP(FileIO_Open)
TEST_PPAPI_NACL_VIA_HTTP(FileIO_AbortCalls)
TEST_PPAPI_NACL_VIA_HTTP(FileIO_ParallelReads)
TEST_PPAPI_NACL_VIA_HTTP(FileIO_ParallelWrites)
TEST_PPAPI_NACL_VIA_HTTP(FileIO_NotAllowMixedReadWrite)
TEST_PPAPI_NACL_VIA_HTTP(MAYBE_FileIO_TouchQuery)
TEST_PPAPI_NACL_VIA_HTTP(MAYBE_FileIO_ReadWriteSetLength)
// The following test requires PPB_FileIO_Trusted, not available in NaCl.
TEST_PPAPI_NACL_VIA_HTTP(DISABLED_FileIO_WillWriteWillSetLength)

TEST_PPAPI_IN_PROCESS_VIA_HTTP(FileRef)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(FileRef)
TEST_PPAPI_NACL_VIA_HTTP(FileRef)

TEST_PPAPI_IN_PROCESS_VIA_HTTP(FileSystem)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(FileSystem)
TEST_PPAPI_NACL_VIA_HTTP(FileSystem)

// Mac/Aura reach NOTIMPLEMENTED/time out.
// Other systems work in-process, but flake out-of-process because of the
// asyncronous nature of the proxy.
// mac: http://crbug.com/96767
// aura: http://crbug.com/104384
// async flakiness:  http://crbug.com/108471
#if defined(OS_MACOSX) || defined(USE_AURA)
#define MAYBE_FlashFullscreen DISABLED_FlashFullscreen
#define MAYBE_OutOfProcessFlashFullscreen DISABLED_FlashFullscreen
#else
#define MAYBE_FlashFullscreen FlashFullscreen
#define MAYBE_OutOfProcessFlashFullscreen FLAKY_FlashFullscreen
#endif

TEST_F(PPAPITest, MAYBE_FlashFullscreen) {
  RunTestViaHTTP("FlashFullscreen");
}
TEST_F(OutOfProcessPPAPITest, MAYBE_OutOfProcessFlashFullscreen) {
  RunTestViaHTTP("FlashFullscreen");
}

// http://crbug.com/107175.
#if defined(OS_MACOSX) || defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_Fullscreen FAILS_Fullscreen
#else
#define MAYBE_Fullscreen Fullscreen
#endif

// TODO(bbudge) Fix fullscreen on Mac.
TEST_PPAPI_IN_PROCESS_VIA_HTTP(MAYBE_Fullscreen)
// TODO(bbudge) Will fail until we add an ACK message to extend user gesture.
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(FAILS_Fullscreen)
// TODO(bbudge) Enable when PPB_Testing_Dev SimulateInputEvent is proxied.
TEST_PPAPI_NACL_VIA_HTTP(DISABLED_Fullscreen)

TEST_PPAPI_IN_PROCESS(FlashClipboard)
TEST_PPAPI_OUT_OF_PROCESS(FlashClipboard)

#if defined(OS_POSIX)
#define MAYBE_DirectoryReader FLAKY_DirectoryReader
#else
#define MAYBE_DirectoryReader DirectoryReader
#endif

// Flaky on Mac + Linux, maybe http://codereview.chromium.org/7094008
// Not implemented out of process: http://crbug.com/106129
TEST_F(PPAPITest, MAYBE_DirectoryReader) {
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

// There is no proxy. This is used for PDF metrics reporting, and PDF only
// runs in process, so there's currently no need for a proxy.
TEST_PPAPI_IN_PROCESS(UMA)

TEST_PPAPI_IN_PROCESS(NetAddressPrivate_AreEqual)
TEST_PPAPI_IN_PROCESS(NetAddressPrivate_AreHostsEqual)
TEST_PPAPI_IN_PROCESS(NetAddressPrivate_Describe)
TEST_PPAPI_IN_PROCESS(NetAddressPrivate_ReplacePort)
TEST_PPAPI_IN_PROCESS(NetAddressPrivate_GetAnyAddress)
TEST_PPAPI_IN_PROCESS(NetAddressPrivate_DescribeIPv6)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_AreEqual)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_AreHostsEqual)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_Describe)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_ReplacePort)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_GetAnyAddress)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_DescribeIPv6)

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

TEST_PPAPI_IN_PROCESS(WebSocket_IsWebSocket)
TEST_PPAPI_IN_PROCESS(WebSocket_UninitializedPropertiesAccess)
TEST_PPAPI_IN_PROCESS(WebSocket_InvalidConnect)
TEST_PPAPI_IN_PROCESS(WebSocket_Protocols)
TEST_PPAPI_IN_PROCESS(WebSocket_GetURL)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_ValidConnect)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_InvalidClose)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_ValidClose)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_GetProtocol)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_TextSendReceive)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_BinarySendReceive)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_CcInterfaces)

TEST_PPAPI_IN_PROCESS(AudioConfig_ValidConfigs)
TEST_PPAPI_IN_PROCESS(AudioConfig_InvalidConfigs)
TEST_PPAPI_OUT_OF_PROCESS(AudioConfig_ValidConfigs)
TEST_PPAPI_OUT_OF_PROCESS(AudioConfig_InvalidConfigs)

TEST_PPAPI_IN_PROCESS(Audio_Creation)
TEST_PPAPI_IN_PROCESS(Audio_DestroyNoStop)
TEST_PPAPI_IN_PROCESS(Audio_Failures)
TEST_PPAPI_OUT_OF_PROCESS(Audio_Creation)
TEST_PPAPI_OUT_OF_PROCESS(Audio_DestroyNoStop)
TEST_PPAPI_OUT_OF_PROCESS(Audio_Failures)

TEST_PPAPI_IN_PROCESS(View_CreateVisible);
TEST_PPAPI_OUT_OF_PROCESS(View_CreateVisible);
TEST_PPAPI_NACL_VIA_HTTP(View_CreateVisible);
// This test ensures that plugins created in a background tab have their
// initial visibility set to false. We don't bother testing in-process for this
// custom test since the out of process code also exercises in-process.
TEST_F(OutOfProcessPPAPITest, View_CreateInvisible) {
  // Make a second tab in the foreground.
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  scoped_refptr<BrowserProxy> browser(tab->GetParentBrowser());
  ASSERT_TRUE(browser.get());
  GURL url = GetTestFileUrl("View_CreatedInvisible");
  ASSERT_TRUE(browser->AppendBackgroundTab(url));

  // Tab 1 will be the one we appended after the default tab 0.
  RunTestURL(tab, url);
}
// This test messes with tab visibility so is custom.
TEST_F(OutOfProcessPPAPITest, View_PageHideShow) {
  GURL url = GetTestFileUrl("View_PageHideShow");
  scoped_refptr<TabProxy> tab = GetActiveTab();
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURLBlockUntilNavigationsComplete(url, 1));

  // The plugin will be loaded in the foreground tab and will set the
  // "created" cookie.
  std::string true_str("TRUE");
  std::string progress = WaitUntilCookieNonEmpty(tab.get(), url,
      "TestPageHideShow:Created",
      TestTimeouts::action_max_timeout_ms());
  ASSERT_EQ(true_str, progress);

  // Make a new tab to cause the original one to hide, this should trigger the
  // next phase of the test.
  scoped_refptr<BrowserProxy> browser(tab->GetParentBrowser());
  ASSERT_TRUE(browser.get());
  ASSERT_TRUE(browser->AppendTab(GURL(chrome::kAboutBlankURL)));

  // Wait until the test acks that it got hidden.
  progress = WaitUntilCookieNonEmpty(tab.get(), url, "TestPageHideShow:Hidden",
      TestTimeouts::action_max_timeout_ms());
  ASSERT_EQ(true_str, progress);

  // Switch back to the test tab.
  ASSERT_TRUE(browser->ActivateTab(0));

  // Wait for the test completion event.
  RunTestURL(tab, url);
}
TEST_PPAPI_IN_PROCESS(View_SizeChange);
TEST_PPAPI_OUT_OF_PROCESS(View_SizeChange);
TEST_PPAPI_NACL_VIA_HTTP(View_SizeChange);
TEST_PPAPI_IN_PROCESS(View_ClipChange);
TEST_PPAPI_OUT_OF_PROCESS(View_ClipChange);
TEST_PPAPI_NACL_VIA_HTTP(View_ClipChange);

TEST_PPAPI_IN_PROCESS(ResourceArray_Basics)
TEST_PPAPI_IN_PROCESS(ResourceArray_OutOfRangeAccess)
TEST_PPAPI_IN_PROCESS(ResourceArray_EmptyArray)
TEST_PPAPI_IN_PROCESS(ResourceArray_InvalidElement)
TEST_PPAPI_OUT_OF_PROCESS(ResourceArray_Basics)
TEST_PPAPI_OUT_OF_PROCESS(ResourceArray_OutOfRangeAccess)
TEST_PPAPI_OUT_OF_PROCESS(ResourceArray_EmptyArray)
TEST_PPAPI_OUT_OF_PROCESS(ResourceArray_InvalidElement)

#endif // ADDRESS_SANITIZER
