// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ppapi_uitest.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"
#include "webkit/plugins/plugin_switches.h"

using content::DomOperationNotificationDetails;
using content::RenderViewHost;

namespace {

// Platform-specific filename relative to the chrome executable.
#if defined(OS_WIN)
const wchar_t library_name[] = L"ppapi_tests.dll";
#elif defined(OS_MACOSX)
const char library_name[] = "ppapi_tests.plugin";
#elif defined(OS_POSIX)
const char library_name[] = "libppapi_tests.so";
#endif

// The large timeout was causing the cycle time for the whole test suite
// to be too long when a tiny bug caused all tests to timeout.
// http://crbug.com/108264
static int kTimeoutMs = 90000;
//static int kTimeoutMs = TestTimeouts::large_test_timeout_ms());

class TestFinishObserver : public content::NotificationObserver {
 public:
  explicit TestFinishObserver(RenderViewHost* render_view_host, int timeout_s)
      : finished_(false), waiting_(false), timeout_s_(timeout_s) {
    registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                   content::Source<RenderViewHost>(render_view_host));
    timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(timeout_s),
                 this, &TestFinishObserver::OnTimeout);
  }

  bool WaitForFinish() {
    if (!finished_) {
      waiting_ = true;
      ui_test_utils::RunMessageLoop();
      waiting_ = false;
    }
    return finished_;
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    DCHECK(type == content::NOTIFICATION_DOM_OPERATION_RESPONSE);
    content::Details<DomOperationNotificationDetails> dom_op_details(details);
    // We might receive responses for other script execution, but we only
    // care about the test finished message.
    std::string response;
    TrimString(dom_op_details->json, "\"", &response);
    if (response == "...") {
      timer_.Stop();
      timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(timeout_s_),
                   this, &TestFinishObserver::OnTimeout);
    } else {
      result_ = response;
      finished_ = true;
      if (waiting_)
        MessageLoopForUI::current()->Quit();
    }
  }

  std::string result() const { return result_; }

  void Reset() {
    finished_ = false;
    waiting_ = false;
    result_.clear();
  }

 private:
  void OnTimeout() {
    MessageLoopForUI::current()->Quit();
  }

  bool finished_;
  bool waiting_;
  int timeout_s_;
  std::string result_;
  content::NotificationRegistrar registrar_;
  base::RepeatingTimer<TestFinishObserver> timer_;

  DISALLOW_COPY_AND_ASSIGN(TestFinishObserver);
};

}  // namespace

PPAPITestBase::PPAPITestBase() {
  EnableDOMAutomation();
}

void PPAPITestBase::SetUpCommandLine(CommandLine* command_line) {
  // The test sends us the result via a cookie.
  command_line->AppendSwitch(switches::kEnableFileCookies);

  // Some stuff is hung off of the testing interface which is not enabled
  // by default.
  command_line->AppendSwitch(switches::kEnablePepperTesting);

  // Smooth scrolling confuses the scrollbar test.
  command_line->AppendSwitch(switches::kDisableSmoothScrolling);
}

GURL PPAPITestBase::GetTestFileUrl(const std::string& test_case) {
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

void PPAPITestBase::RunTest(const std::string& test_case) {
  GURL url = GetTestFileUrl(test_case);
  RunTestURL(url);
}

void PPAPITestBase::RunTestAndReload(const std::string& test_case) {
  GURL url = GetTestFileUrl(test_case);
  RunTestURL(url);
  // If that passed, we simply run the test again, which navigates again.
  RunTestURL(url);
}

void PPAPITestBase::RunTestViaHTTP(const std::string& test_case) {
  FilePath document_root;
  ASSERT_TRUE(GetHTTPDocumentRoot(&document_root));
  RunHTTPTestServer(document_root, test_case, "");
}

void PPAPITestBase::RunTestWithSSLServer(const std::string& test_case) {
  FilePath document_root;
  ASSERT_TRUE(GetHTTPDocumentRoot(&document_root));
  net::TestServer test_server(net::BaseTestServer::HTTPSOptions(),
                              document_root);
  ASSERT_TRUE(test_server.Start());
  uint16_t port = test_server.host_port_pair().port();
  RunHTTPTestServer(document_root, test_case,
                    StringPrintf("ssl_server_port=%d", port));
}

void PPAPITestBase::RunTestWithWebSocketServer(const std::string& test_case) {
  FilePath websocket_root_dir;
  ASSERT_TRUE(
      PathService::Get(content::DIR_LAYOUT_TESTS, &websocket_root_dir));
  ui_test_utils::TestWebSocketServer server;
  int port = server.UseRandomPort();
  ASSERT_TRUE(server.Start(websocket_root_dir));
  FilePath http_document_root;
  ASSERT_TRUE(GetHTTPDocumentRoot(&http_document_root));
  RunHTTPTestServer(http_document_root, test_case,
                    StringPrintf("websocket_port=%d", port));
}

std::string PPAPITestBase::StripPrefixes(const std::string& test_name) {
  const char* const prefixes[] = {
      "FAILS_", "FLAKY_", "DISABLED_", "SLOW_" };
  for (size_t i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); ++i)
    if (test_name.find(prefixes[i]) == 0)
      return test_name.substr(strlen(prefixes[i]));
  return test_name;
}

void PPAPITestBase::RunTestURL(const GURL& test_url) {
  // See comment above TestingInstance in ppapi/test/testing_instance.h.
  // Basically it sends messages using the DOM automation controller. The
  // value of "..." means it's still working and we should continue to wait,
  // any other value indicates completion (in this case it will start with
  // "PASS" or "FAIL"). This keeps us from timing out on waits for long tests.
  TestFinishObserver observer(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), kTimeoutMs);

  ui_test_utils::NavigateToURL(browser(), test_url);

  ASSERT_TRUE(observer.WaitForFinish()) << "Test timed out.";

  EXPECT_STREQ("PASS", observer.result().c_str());
}

void PPAPITestBase::RunHTTPTestServer(
    const FilePath& document_root,
    const std::string& test_case,
    const std::string& extra_params) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              net::TestServer::kLocalhost,
                              document_root);
  ASSERT_TRUE(test_server.Start());
  std::string query = BuildQuery("files/test_case.html?", test_case);
  if (!extra_params.empty())
    query = StringPrintf("%s&%s", query.c_str(), extra_params.c_str());

  GURL url = test_server.GetURL(query);
  RunTestURL(url);
}

bool PPAPITestBase::GetHTTPDocumentRoot(FilePath* document_root) {
  // For HTTP tests, we use the output DIR to grab the generated files such
  // as the NEXEs.
  FilePath exe_dir = CommandLine::ForCurrentProcess()->GetProgram().DirName();
  FilePath src_dir;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &src_dir))
    return false;

  // TestServer expects a path relative to source. So we must first
  // generate absolute paths to SRC and EXE and from there generate
  // a relative path.
  if (!exe_dir.IsAbsolute()) file_util::AbsolutePath(&exe_dir);
  if (!src_dir.IsAbsolute()) file_util::AbsolutePath(&src_dir);
  if (!exe_dir.IsAbsolute())
    return false;
  if (!src_dir.IsAbsolute())
    return false;

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
  for (size_t tmp_itr = match; tmp_itr < src_size; ++tmp_itr) {
    *document_root = document_root->Append(FILE_PATH_LITERAL(".."));
  }
  for (; match < exe_size; ++match) {
    *document_root = document_root->Append(exe_parts[match]);
  }
  return true;
}

PPAPITest::PPAPITest() {
}

void PPAPITest::SetUpCommandLine(CommandLine* command_line) {
  PPAPITestBase::SetUpCommandLine(command_line);

  // Append the switch to register the pepper plugin.
  // library name = <out dir>/<test_name>.<library_extension>
  // MIME type = application/x-ppapi-<test_name>
  FilePath plugin_dir;
  EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));

  FilePath plugin_lib = plugin_dir.Append(library_name);
  EXPECT_TRUE(file_util::PathExists(plugin_lib));
  FilePath::StringType pepper_plugin = plugin_lib.value();
  pepper_plugin.append(FILE_PATH_LITERAL(";application/x-ppapi-tests"));
  command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                   pepper_plugin);
  command_line->AppendSwitchASCII(switches::kAllowNaClSocketAPI, "127.0.0.1");
}

std::string PPAPITest::BuildQuery(const std::string& base,
                                  const std::string& test_case){
  return StringPrintf("%stestcase=%s", base.c_str(), test_case.c_str());
}

OutOfProcessPPAPITest::OutOfProcessPPAPITest() {
}

void OutOfProcessPPAPITest::SetUpCommandLine(CommandLine* command_line) {
  PPAPITest::SetUpCommandLine(command_line);

  // Run PPAPI out-of-process to exercise proxy implementations.
  command_line->AppendSwitch(switches::kPpapiOutOfProcess);
}

PPAPINaClTest::PPAPINaClTest() {
}

void PPAPINaClTest::SetUpCommandLine(CommandLine* command_line) {
  PPAPITestBase::SetUpCommandLine(command_line);

  FilePath plugin_lib;
  EXPECT_TRUE(PathService::Get(chrome::FILE_NACL_PLUGIN, &plugin_lib));
  EXPECT_TRUE(file_util::PathExists(plugin_lib));

  // Enable running NaCl outside of the store.
  command_line->AppendSwitch(switches::kEnableNaCl);
  command_line->AppendSwitchASCII(switches::kAllowNaClSocketAPI, "127.0.0.1");
}

// Append the correct mode and testcase string
std::string PPAPINaClTest::BuildQuery(const std::string& base,
                                      const std::string& test_case) {
  return StringPrintf("%smode=nacl&testcase=%s", base.c_str(),
                      test_case.c_str());
}

PPAPINaClTestDisallowedSockets::PPAPINaClTestDisallowedSockets() {
}

void PPAPINaClTestDisallowedSockets::SetUpCommandLine(
    CommandLine* command_line) {
  PPAPITestBase::SetUpCommandLine(command_line);

  FilePath plugin_lib;
  EXPECT_TRUE(PathService::Get(chrome::FILE_NACL_PLUGIN, &plugin_lib));
  EXPECT_TRUE(file_util::PathExists(plugin_lib));

  // Enable running NaCl outside of the store.
  command_line->AppendSwitch(switches::kEnableNaCl);
}

// Append the correct mode and testcase string
std::string PPAPINaClTestDisallowedSockets::BuildQuery(
    const std::string& base,
    const std::string& test_case) {
  return StringPrintf("%smode=nacl&testcase=%s", base.c_str(),
                      test_case.c_str());
}

// This macro finesses macro expansion to do what we want.
#define STRIP_PREFIXES(test_name) StripPrefixes(#test_name)

// Use these macros to run the tests for a specific interface.
// Most interfaces should be tested with both macros.
#define TEST_PPAPI_IN_PROCESS(test_name) \
    IN_PROC_BROWSER_TEST_F(PPAPITest, test_name) { \
      RunTest(STRIP_PREFIXES(test_name)); \
    }
#define TEST_PPAPI_OUT_OF_PROCESS(test_name) \
    IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, test_name) { \
      RunTest(STRIP_PREFIXES(test_name)); \
    }

// Similar macros that test over HTTP.
#define TEST_PPAPI_IN_PROCESS_VIA_HTTP(test_name) \
    IN_PROC_BROWSER_TEST_F(PPAPITest, test_name) { \
      RunTestViaHTTP(STRIP_PREFIXES(test_name)); \
    }
#define TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(test_name) \
    IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, test_name) { \
      RunTestViaHTTP(STRIP_PREFIXES(test_name)); \
    }

// Similar macros that test with an SSL server.
#define TEST_PPAPI_IN_PROCESS_WITH_SSL_SERVER(test_name) \
    IN_PROC_BROWSER_TEST_F(PPAPITest, test_name) { \
      RunTestWithSSLServer(STRIP_PREFIXES(test_name)); \
    }
#define TEST_PPAPI_OUT_OF_PROCESS_WITH_SSL_SERVER(test_name) \
    IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, test_name) { \
      RunTestWithSSLServer(STRIP_PREFIXES(test_name)); \
    }

// Similar macros that test with WebSocket server
#define TEST_PPAPI_IN_PROCESS_WITH_WS(test_name) \
    IN_PROC_BROWSER_TEST_F(PPAPITest, test_name) { \
      RunTestWithWebSocketServer(STRIP_PREFIXES(test_name)); \
    }
#define TEST_PPAPI_OUT_OF_PROCESS_WITH_WS(test_name) \
    IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, test_name) { \
      RunTestWithWebSocketServer(STRIP_PREFIXES(test_name)); \
    }


#if defined(DISABLE_NACL)
#define TEST_PPAPI_NACL_VIA_HTTP(test_name)
#define TEST_PPAPI_NACL_VIA_HTTP_DISALLOWED_SOCKETS(test_name)
#define TEST_PPAPI_NACL_WITH_SSL_SERVER(test_name)
#define TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(test_name)
#else

// NaCl based PPAPI tests
#define TEST_PPAPI_NACL_VIA_HTTP(test_name) \
    IN_PROC_BROWSER_TEST_F(PPAPINaClTest, test_name) { \
      RunTestViaHTTP(STRIP_PREFIXES(test_name)); \
    }

// NaCl based PPAPI tests with disallowed socket API
#define TEST_PPAPI_NACL_VIA_HTTP_DISALLOWED_SOCKETS(test_name) \
    IN_PROC_BROWSER_TEST_F(PPAPINaClTestDisallowedSockets, test_name) { \
      RunTestViaHTTP(STRIP_PREFIXES(test_name)); \
    }

// NaCl based PPAPI tests with SSL server
#define TEST_PPAPI_NACL_WITH_SSL_SERVER(test_name) \
    IN_PROC_BROWSER_TEST_F(PPAPINaClTest, test_name) { \
      RunTestWithSSLServer(STRIP_PREFIXES(test_name)); \
    }

// NaCl based PPAPI tests with WebSocket server
#define TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(test_name) \
    IN_PROC_BROWSER_TEST_F(PPAPINaClTest, test_name) { \
      RunTestWithWebSocketServer(STRIP_PREFIXES(test_name)); \
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
// Flaky, http://crbug.com/111355
TEST_PPAPI_OUT_OF_PROCESS(DISABLED_Broker)

TEST_PPAPI_IN_PROCESS(Core)
TEST_PPAPI_OUT_OF_PROCESS(Core)

// Times out on Linux. http://crbug.com/108859
#if defined(OS_LINUX)
#define MAYBE_InputEvent DISABLED_InputEvent
#elif defined(OS_MACOSX)
// Flaky on Mac. http://crbug.com/109258
#define MAYBE_InputEvent DISABLED_InputEvent
#else
#define MAYBE_InputEvent InputEvent
#endif

TEST_PPAPI_IN_PROCESS(MAYBE_InputEvent)
TEST_PPAPI_OUT_OF_PROCESS(MAYBE_InputEvent)
// TODO(bbudge) Enable when input events are proxied correctly for NaCl.
TEST_PPAPI_NACL_VIA_HTTP(DISABLED_InputEvent)

TEST_PPAPI_IN_PROCESS(Instance_ExecuteScript);
TEST_PPAPI_OUT_OF_PROCESS(Instance_ExecuteScript)
// ExecuteScript isn't supported by NaCl.

// We run and reload the RecursiveObjects test to ensure that the InstanceObject
// (and others) are properly cleaned up after the first run.
IN_PROC_BROWSER_TEST_F(PPAPITest, Instance_RecursiveObjects) {
  RunTestAndReload("Instance_RecursiveObjects");
}
// TODO(dmichael): Make it work out-of-process (or at least see whether we
//                 care).
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest,
                       DISABLED_Instance_RecursiveObjects) {
  RunTestAndReload("Instance_RecursiveObjects");
}
TEST_PPAPI_IN_PROCESS(Instance_TestLeakedObjectDestructors);
TEST_PPAPI_OUT_OF_PROCESS(Instance_TestLeakedObjectDestructors);
// ScriptableObjects aren't supported in NaCl, so Instance_RecursiveObjects and
// Instance_TestLeakedObjectDestructors don't make sense for NaCl.

TEST_PPAPI_IN_PROCESS(Graphics2D)
TEST_PPAPI_OUT_OF_PROCESS(Graphics2D)
TEST_PPAPI_NACL_VIA_HTTP(Graphics2D)

TEST_PPAPI_IN_PROCESS(ImageData)
TEST_PPAPI_OUT_OF_PROCESS(ImageData)
TEST_PPAPI_NACL_VIA_HTTP(ImageData)

TEST_PPAPI_IN_PROCESS(BrowserFont)
TEST_PPAPI_OUT_OF_PROCESS(BrowserFont)

TEST_PPAPI_IN_PROCESS(Buffer)
TEST_PPAPI_OUT_OF_PROCESS(Buffer)

TEST_PPAPI_OUT_OF_PROCESS_WITH_SSL_SERVER(TCPSocketPrivate)
TEST_PPAPI_IN_PROCESS_WITH_SSL_SERVER(TCPSocketPrivate)
TEST_PPAPI_NACL_WITH_SSL_SERVER(TCPSocketPrivate)

TEST_PPAPI_OUT_OF_PROCESS_WITH_SSL_SERVER(TCPSocketPrivateTrusted)
TEST_PPAPI_IN_PROCESS_WITH_SSL_SERVER(TCPSocketPrivateTrusted)

TEST_PPAPI_IN_PROCESS_VIA_HTTP(UDPSocketPrivate)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(UDPSocketPrivate)
TEST_PPAPI_NACL_VIA_HTTP(UDPSocketPrivate)

TEST_PPAPI_NACL_VIA_HTTP_DISALLOWED_SOCKETS(TCPServerSocketPrivateDisallowed)
TEST_PPAPI_NACL_VIA_HTTP_DISALLOWED_SOCKETS(TCPSocketPrivateDisallowed)
TEST_PPAPI_NACL_VIA_HTTP_DISALLOWED_SOCKETS(UDPSocketPrivateDisallowed)

TEST_PPAPI_IN_PROCESS_VIA_HTTP(TCPServerSocketPrivate)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(TCPServerSocketPrivate)
TEST_PPAPI_NACL_VIA_HTTP(TCPServerSocketPrivate)

TEST_PPAPI_IN_PROCESS_VIA_HTTP(HostResolverPrivate_Create)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(HostResolverPrivate_Resolve)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(HostResolverPrivate_ResolveIPV4)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(HostResolverPrivate_Create)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(HostResolverPrivate_Resolve)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(HostResolverPrivate_ResolveIPV4)
TEST_PPAPI_NACL_VIA_HTTP(HostResolverPrivate_Create)
TEST_PPAPI_NACL_VIA_HTTP(HostResolverPrivate_Resolve)
TEST_PPAPI_NACL_VIA_HTTP(HostResolverPrivate_ResolveIPV4)

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
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLLoader_PrefetchBufferThreshold)

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

// URLRequestInfo tests.
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLRequest_CreateAndIsURLRequestInfo)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLRequest_CreateAndIsURLRequestInfo)
TEST_PPAPI_NACL_VIA_HTTP(URLRequest_CreateAndIsURLRequestInfo)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLRequest_SetProperty)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLRequest_SetProperty)
TEST_PPAPI_NACL_VIA_HTTP(URLRequest_SetProperty)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLRequest_AppendDataToBody)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLRequest_AppendDataToBody)
TEST_PPAPI_NACL_VIA_HTTP(URLRequest_AppendDataToBody)
TEST_PPAPI_IN_PROCESS_VIA_HTTP(URLRequest_Stress)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLRequest_Stress)
TEST_PPAPI_NACL_VIA_HTTP(URLRequest_Stress)

TEST_PPAPI_IN_PROCESS(PaintAggregator)
TEST_PPAPI_OUT_OF_PROCESS(PaintAggregator)
TEST_PPAPI_NACL_VIA_HTTP(PaintAggregator)

// TODO(danakj): http://crbug.com/115286
TEST_PPAPI_IN_PROCESS(DISABLED_Scrollbar)
// http://crbug.com/89961
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, FAILS_Scrollbar) {
  RunTest("Scrollbar");
}
// TODO(danakj): http://crbug.com/115286
TEST_PPAPI_NACL_VIA_HTTP(DISABLED_Scrollbar)

TEST_PPAPI_IN_PROCESS(URLUtil)
TEST_PPAPI_OUT_OF_PROCESS(URLUtil)

TEST_PPAPI_IN_PROCESS(CharSet)
TEST_PPAPI_OUT_OF_PROCESS(CharSet)

TEST_PPAPI_IN_PROCESS(Crypto)
TEST_PPAPI_OUT_OF_PROCESS(Crypto)

TEST_PPAPI_IN_PROCESS(Var)
TEST_PPAPI_OUT_OF_PROCESS(Var)
TEST_PPAPI_NACL_VIA_HTTP(Var)

// Flaky on mac, http://crbug.com/121107
#if defined(OS_MACOSX)
#define MAYBE_VarDeprecated DISABLED_VarDeprecated
#else
#define MAYBE_VarDeprecated VarDeprecated
#endif

TEST_PPAPI_IN_PROCESS(VarDeprecated)
TEST_PPAPI_OUT_OF_PROCESS(MAYBE_VarDeprecated)

// Windows defines 'PostMessage', so we have to undef it.
#ifdef PostMessage
#undef PostMessage
#endif
TEST_PPAPI_IN_PROCESS(PostMessage_SendInInit)
TEST_PPAPI_IN_PROCESS(PostMessage_SendingData)
// TODO(danakj): http://crbug.com/115286
TEST_PPAPI_IN_PROCESS(DISABLED_PostMessage_SendingArrayBuffer)
TEST_PPAPI_IN_PROCESS(PostMessage_MessageEvent)
TEST_PPAPI_IN_PROCESS(PostMessage_NoHandler)
TEST_PPAPI_IN_PROCESS(PostMessage_ExtraParam)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_SendInInit)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_SendingData)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_SendingArrayBuffer)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_MessageEvent)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_NoHandler)
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_ExtraParam)
#if !defined(OS_WIN) && !(defined(OS_LINUX) && defined(ARCH_CPU_64_BITS))
// Times out on Windows XP, Windows 7, and Linux x64: http://crbug.com/95557
TEST_PPAPI_OUT_OF_PROCESS(PostMessage_NonMainThread)
#endif
TEST_PPAPI_NACL_VIA_HTTP(PostMessage_SendInInit)
TEST_PPAPI_NACL_VIA_HTTP(PostMessage_SendingData)
TEST_PPAPI_NACL_VIA_HTTP(SLOW_PostMessage_SendingArrayBuffer)
TEST_PPAPI_NACL_VIA_HTTP(PostMessage_MessageEvent)
TEST_PPAPI_NACL_VIA_HTTP(PostMessage_NoHandler)

#if defined(OS_WIN)
// Flaky: http://crbug.com/111209
//
// Note from sheriffs miket and syzm: we're not convinced that this test is
// directly to blame for the flakiness. It's possible that it's a more general
// problem that is exposing itself only with one of the later tests in this
// series.
TEST_PPAPI_NACL_VIA_HTTP(DISABLED_PostMessage_ExtraParam)
#else
TEST_PPAPI_NACL_VIA_HTTP(PostMessage_ExtraParam)
#endif

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

// PPAPINaclTest.FileIO_ParallelReads is flaky on Mac. http://crbug.com/121104
#if defined(OS_MACOSX)
#define MAYBE_FileIO_ParallelReads DISABLED_FileIO_ParallelReads
#else
#define MAYBE_FileIO_ParallelReads FileIO_ParallelReads
#endif

TEST_PPAPI_NACL_VIA_HTTP(FileIO_Open)
TEST_PPAPI_NACL_VIA_HTTP(FileIO_AbortCalls)
TEST_PPAPI_NACL_VIA_HTTP(MAYBE_FileIO_ParallelReads)
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
#define MAYBE_OutOfProcessFlashFullscreen FlashFullscreen
#endif

IN_PROC_BROWSER_TEST_F(PPAPITest, MAYBE_FlashFullscreen) {
  RunTestViaHTTP("FlashFullscreen");
}
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, MAYBE_OutOfProcessFlashFullscreen) {
  RunTestViaHTTP("FlashFullscreen");
}

TEST_PPAPI_IN_PROCESS_VIA_HTTP(Fullscreen)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(Fullscreen)

TEST_PPAPI_IN_PROCESS(FlashClipboard)
TEST_PPAPI_OUT_OF_PROCESS(FlashClipboard)

TEST_PPAPI_IN_PROCESS(X509CertificatePrivate)
TEST_PPAPI_OUT_OF_PROCESS(X509CertificatePrivate)

// http://crbug.com/63239
#if defined(OS_POSIX)
#define MAYBE_DirectoryReader DISABLED_DirectoryReader
#else
#define MAYBE_DirectoryReader DirectoryReader
#endif

// Flaky on Mac + Linux, maybe http://codereview.chromium.org/7094008
// Not implemented out of process: http://crbug.com/106129
IN_PROC_BROWSER_TEST_F(PPAPITest, MAYBE_DirectoryReader) {
  RunTestViaHTTP("DirectoryReader");
}

#if defined(ENABLE_P2P_APIS)
// Flaky. http://crbug.com/84294
IN_PROC_BROWSER_TEST_F(PPAPITest, DISABLED_Transport) {
  RunTest("Transport");
}
// http://crbug.com/89961
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, DISABLED_Transport) {
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
TEST_PPAPI_IN_PROCESS(NetAddressPrivate_GetFamily)
TEST_PPAPI_IN_PROCESS(NetAddressPrivate_GetPort)
TEST_PPAPI_IN_PROCESS(NetAddressPrivate_GetAddress)
TEST_PPAPI_IN_PROCESS(NetAddressPrivate_GetScopeID)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_AreEqual)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_AreHostsEqual)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_Describe)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_ReplacePort)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_GetAnyAddress)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_DescribeIPv6)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_GetFamily)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_GetPort)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_GetAddress)
TEST_PPAPI_OUT_OF_PROCESS(NetAddressPrivate_GetScopeID)

// Frequently timing out on Windows. http://crbug.com/115440
#if defined(OS_WIN)
#define MAYBE_NetAddressPrivateUntrusted_Describe \
  DISABLED_NetAddressPrivateUntrusted_Describe
#define MAYBE_NetAddressPrivateUntrusted_ReplacePort \
  DISABLED_NetAddressPrivateUntrusted_ReplacePort
#define MAYBE_NetAddressPrivateUntrusted_GetPort \
  DISABLED_NetAddressPrivateUntrusted_GetPort
#else
#define MAYBE_NetAddressPrivateUntrusted_Describe \
  NetAddressPrivateUntrusted_Describe
#define MAYBE_NetAddressPrivateUntrusted_ReplacePort \
  NetAddressPrivateUntrusted_ReplacePort
#define MAYBE_NetAddressPrivateUntrusted_GetPort \
  NetAddressPrivateUntrusted_GetPort
#endif

TEST_PPAPI_NACL_VIA_HTTP(NetAddressPrivateUntrusted_AreEqual)
TEST_PPAPI_NACL_VIA_HTTP(NetAddressPrivateUntrusted_AreHostsEqual)
TEST_PPAPI_NACL_VIA_HTTP(MAYBE_NetAddressPrivateUntrusted_Describe)
TEST_PPAPI_NACL_VIA_HTTP(MAYBE_NetAddressPrivateUntrusted_ReplacePort)
TEST_PPAPI_NACL_VIA_HTTP(NetAddressPrivateUntrusted_GetAnyAddress)
TEST_PPAPI_NACL_VIA_HTTP(NetAddressPrivateUntrusted_GetFamily)
TEST_PPAPI_NACL_VIA_HTTP(MAYBE_NetAddressPrivateUntrusted_GetPort)
TEST_PPAPI_NACL_VIA_HTTP(NetAddressPrivateUntrusted_GetAddress)
TEST_PPAPI_NACL_VIA_HTTP(NetAddressPrivateUntrusted_GetScopeID)

TEST_PPAPI_IN_PROCESS(NetworkMonitorPrivate_Basic)
TEST_PPAPI_OUT_OF_PROCESS(NetworkMonitorPrivate_Basic)
TEST_PPAPI_IN_PROCESS(NetworkMonitorPrivate_2Monitors)
TEST_PPAPI_OUT_OF_PROCESS(NetworkMonitorPrivate_2Monitors)
TEST_PPAPI_IN_PROCESS(NetworkMonitorPrivate_DeleteInCallback)
TEST_PPAPI_OUT_OF_PROCESS(NetworkMonitorPrivate_DeleteInCallback)
TEST_PPAPI_IN_PROCESS(NetworkMonitorPrivate_ListObserver)
TEST_PPAPI_OUT_OF_PROCESS(NetworkMonitorPrivate_ListObserver)

TEST_PPAPI_IN_PROCESS(Flash_SetInstanceAlwaysOnTop)
TEST_PPAPI_IN_PROCESS(Flash_GetProxyForURL)
TEST_PPAPI_IN_PROCESS(Flash_MessageLoop)
TEST_PPAPI_IN_PROCESS(Flash_GetLocalTimeZoneOffset)
TEST_PPAPI_IN_PROCESS(Flash_GetCommandLineArgs)
TEST_PPAPI_IN_PROCESS(Flash_GetDeviceID)
TEST_PPAPI_OUT_OF_PROCESS(Flash_SetInstanceAlwaysOnTop)
TEST_PPAPI_OUT_OF_PROCESS(Flash_GetProxyForURL)
TEST_PPAPI_OUT_OF_PROCESS(Flash_MessageLoop)
TEST_PPAPI_OUT_OF_PROCESS(Flash_GetLocalTimeZoneOffset)
TEST_PPAPI_OUT_OF_PROCESS(Flash_GetCommandLineArgs)
TEST_PPAPI_OUT_OF_PROCESS(Flash_GetDeviceID)

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
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_StressedSendReceive)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_BufferedAmount)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_AbortCalls)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_CcInterfaces)
TEST_PPAPI_IN_PROCESS(WebSocket_UtilityInvalidConnect)
TEST_PPAPI_IN_PROCESS(WebSocket_UtilityProtocols)
TEST_PPAPI_IN_PROCESS(WebSocket_UtilityGetURL)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_UtilityValidConnect)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_UtilityInvalidClose)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_UtilityValidClose)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_UtilityGetProtocol)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_UtilityTextSendReceive)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_UtilityBinarySendReceive)
TEST_PPAPI_IN_PROCESS_WITH_WS(WebSocket_UtilityBufferedAmount)
TEST_PPAPI_NACL_VIA_HTTP(WebSocket_IsWebSocket)
TEST_PPAPI_NACL_VIA_HTTP(WebSocket_UninitializedPropertiesAccess)
TEST_PPAPI_NACL_VIA_HTTP(WebSocket_InvalidConnect)
TEST_PPAPI_NACL_VIA_HTTP(WebSocket_Protocols)
TEST_PPAPI_NACL_VIA_HTTP(WebSocket_GetURL)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_ValidConnect)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_InvalidClose)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_ValidClose)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_GetProtocol)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_TextSendReceive)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_BinarySendReceive)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_StressedSendReceive)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_BufferedAmount)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_AbortCalls)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_CcInterfaces)
TEST_PPAPI_NACL_VIA_HTTP(WebSocket_UtilityInvalidConnect)
TEST_PPAPI_NACL_VIA_HTTP(WebSocket_UtilityProtocols)
TEST_PPAPI_NACL_VIA_HTTP(WebSocket_UtilityGetURL)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_UtilityValidConnect)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_UtilityInvalidClose)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_UtilityValidClose)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_UtilityGetProtocol)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_UtilityTextSendReceive)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_UtilityBinarySendReceive)
TEST_PPAPI_NACL_VIA_HTTP_WITH_WS(WebSocket_UtilityBufferedAmount)

TEST_PPAPI_IN_PROCESS(AudioConfig_ValidConfigs)
TEST_PPAPI_IN_PROCESS(AudioConfig_InvalidConfigs)
TEST_PPAPI_OUT_OF_PROCESS(AudioConfig_ValidConfigs)
TEST_PPAPI_OUT_OF_PROCESS(AudioConfig_InvalidConfigs)

// PPAPITest.Audio_Creation fails on Linux & Mac try servers.
// http://crbug.com/114712
#if defined(OS_LINUX) || defined(OS_MACOSX)
#define MAYBE_Audio_Creation DISABLED_Audio_Creation
#else
#define MAYBE_Audio_Creation Audio_Creation
#endif

TEST_PPAPI_IN_PROCESS(MAYBE_Audio_Creation)
TEST_PPAPI_IN_PROCESS(Audio_DestroyNoStop)
TEST_PPAPI_IN_PROCESS(Audio_Failures)
TEST_PPAPI_OUT_OF_PROCESS(MAYBE_Audio_Creation)
TEST_PPAPI_OUT_OF_PROCESS(Audio_DestroyNoStop)
TEST_PPAPI_OUT_OF_PROCESS(Audio_Failures)
TEST_PPAPI_NACL_VIA_HTTP(MAYBE_Audio_Creation)
TEST_PPAPI_NACL_VIA_HTTP(Audio_DestroyNoStop)
TEST_PPAPI_NACL_VIA_HTTP(Audio_Failures)

TEST_PPAPI_IN_PROCESS(View_CreateVisible);
TEST_PPAPI_OUT_OF_PROCESS(View_CreateVisible);
TEST_PPAPI_NACL_VIA_HTTP(View_CreateVisible);
// This test ensures that plugins created in a background tab have their
// initial visibility set to false. We don't bother testing in-process for this
// custom test since the out of process code also exercises in-process.

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, View_CreateInvisible) {
  // Make a second tab in the foreground.
  GURL url = GetTestFileUrl("View_CreatedInvisible");
  browser::NavigateParams params(browser(), url, content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_BACKGROUND_TAB;
  ui_test_utils::NavigateToURL(&params);
}

// This test messes with tab visibility so is custom.
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, View_PageHideShow) {
  // The plugin will be loaded in the foreground tab and will send us a message.
  TestFinishObserver observer(
      browser()->GetSelectedWebContents()->GetRenderViewHost(),
      TestTimeouts::action_max_timeout_ms());

  GURL url = GetTestFileUrl("View_PageHideShow");
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_TRUE(observer.WaitForFinish()) << "Test timed out.";
  EXPECT_STREQ("TestPageHideShow:Created", observer.result().c_str());
  observer.Reset();

  // Make a new tab to cause the original one to hide, this should trigger the
  // next phase of the test.
  browser::NavigateParams params(
      browser(), GURL(chrome::kAboutBlankURL), content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  ui_test_utils::NavigateToURL(&params);

  // Wait until the test acks that it got hidden.
  ASSERT_TRUE(observer.WaitForFinish()) << "Test timed out.";
  EXPECT_STREQ("TestPageHideShow:Hidden", observer.result().c_str());

  // Wait for the test completion event.
  observer.Reset();

  // Switch back to the test tab.
  browser()->ActivateTabAt(0, true);

  ASSERT_TRUE(observer.WaitForFinish()) << "Test timed out.";
  EXPECT_STREQ("PASS", observer.result().c_str());
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

TEST_PPAPI_IN_PROCESS(FlashMessageLoop_Basics)
TEST_PPAPI_IN_PROCESS(FlashMessageLoop_RunWithoutQuit)
TEST_PPAPI_OUT_OF_PROCESS(FlashMessageLoop_Basics)
TEST_PPAPI_OUT_OF_PROCESS(FlashMessageLoop_RunWithoutQuit)

TEST_PPAPI_IN_PROCESS(MouseCursor)
TEST_PPAPI_OUT_OF_PROCESS(MouseCursor)
TEST_PPAPI_NACL_VIA_HTTP(MouseCursor)

#endif // ADDRESS_SANITIZER
