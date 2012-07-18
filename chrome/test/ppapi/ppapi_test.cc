// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ppapi/ppapi_test.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/test/gpu/test_switches.h"
#include "media/audio/audio_manager.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"
#include "ui/gl/gl_switches.h"
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

bool IsAudioOutputAvailable() {
  scoped_ptr<media::AudioManager> audio_manager(media::AudioManager::Create());
  return audio_manager->HasAudioOutputDevices();
}

}  // namespace

PPAPITestBase::TestFinishObserver::TestFinishObserver(
    RenderViewHost* render_view_host,
    int timeout_s)
    : finished_(false),
      waiting_(false),
      timeout_s_(timeout_s) {
  registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                 content::Source<RenderViewHost>(render_view_host));
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(timeout_s),
               this, &TestFinishObserver::OnTimeout);
}

bool PPAPITestBase::TestFinishObserver::WaitForFinish() {
  if (!finished_) {
    waiting_ = true;
    ui_test_utils::RunMessageLoop();
    waiting_ = false;
  }
  return finished_;
}

void PPAPITestBase::TestFinishObserver::Observe(
    int type,
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

void PPAPITestBase::TestFinishObserver::Reset() {
  finished_ = false;
  waiting_ = false;
  result_.clear();
}

void PPAPITestBase::TestFinishObserver::OnTimeout() {
  MessageLoopForUI::current()->Quit();
}

PPAPITestBase::PPAPITestBase() {
}

void PPAPITestBase::SetUpCommandLine(CommandLine* command_line) {
  // Do not use mesa if real GPU is required.
  if (!command_line->HasSwitch(switches::kUseGpuInTests)) {
#if !defined(OS_MACOSX)
    CHECK(test_launcher_utils::OverrideGLImplementation(
        command_line, gfx::kGLImplementationOSMesaName)) <<
        "kUseGL must not be set by test framework code!";
#endif
  }

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

void PPAPITestBase::RunTestIfAudioOutputAvailable(
    const std::string& test_case) {
  if (IsAudioOutputAvailable()) {
    RunTest(test_case);
  } else {
    LOG(WARNING) << "PPAPITest: " << test_case <<
        " was not executed because there are no audio devices available.";
  }
}

void PPAPITestBase::RunTestViaHTTPIfAudioOutputAvailable(
    const std::string& test_case) {
  if (IsAudioOutputAvailable()) {
    RunTestViaHTTP(test_case);
  } else {
    LOG(WARNING) << "PPAPITest: " << test_case <<
        " was not executed because there are no audio devices available.";
  }
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
      chrome::GetActiveWebContents(browser())->GetRenderViewHost(), kTimeoutMs);

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
std::string PPAPINaClNewlibTest::BuildQuery(const std::string& base,
                                      const std::string& test_case) {
  return StringPrintf("%smode=nacl_newlib&testcase=%s", base.c_str(),
                      test_case.c_str());
}

// Append the correct mode and testcase string
std::string PPAPINaClGLibcTest::BuildQuery(const std::string& base,
                                      const std::string& test_case) {
  return StringPrintf("%smode=nacl_glibc&testcase=%s", base.c_str(),
                      test_case.c_str());
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
  return StringPrintf("%smode=nacl_newlib&testcase=%s", base.c_str(),
                      test_case.c_str());
}

