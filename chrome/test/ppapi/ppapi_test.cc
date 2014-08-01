// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ppapi/ppapi_test.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "media/base/media_switches.h"
#include "net/base/filename_util.h"
#include "net/base/test_data_directory.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "ppapi/shared_impl/test_harness_utils.h"
#include "ui/gl/gl_switches.h"

using content::DomOperationNotificationDetails;
using content::RenderViewHost;
using content::TestMessageHandler;

namespace {

void AddPrivateSwitches(base::CommandLine* command_line) {
  // For TestRequestOSFileHandle.
  command_line->AppendSwitch(switches::kUnlimitedStorage);
  command_line->AppendSwitchASCII(switches::kAllowNaClFileHandleAPI,
                                  "127.0.0.1");
}

}  // namespace

PPAPITestMessageHandler::PPAPITestMessageHandler() {
}

TestMessageHandler::MessageResponse PPAPITestMessageHandler::HandleMessage(
    const std::string& json) {
  std::string trimmed;
  base::TrimString(json, "\"", &trimmed);
  if (trimmed == "...")
    return CONTINUE;
  message_ = trimmed;
  return DONE;
}

void PPAPITestMessageHandler::Reset() {
  TestMessageHandler::Reset();
  message_.clear();
}

PPAPITestBase::InfoBarObserver::InfoBarObserver(PPAPITestBase* test_base)
    : test_base_(test_base),
      expecting_infobar_(false),
      should_accept_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
                 content::NotificationService::AllSources());
}

PPAPITestBase::InfoBarObserver::~InfoBarObserver() {
  EXPECT_FALSE(expecting_infobar_) << "Missing an expected infobar";
}

void PPAPITestBase::InfoBarObserver::ExpectInfoBarAndAccept(
    bool should_accept) {
  ASSERT_FALSE(expecting_infobar_);
  expecting_infobar_ = true;
  should_accept_ = should_accept;
}

void PPAPITestBase::InfoBarObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  ASSERT_EQ(chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED, type);
  // It's not safe to remove the infobar here, since other observers (e.g. the
  // InfoBarContainer) may still need to access it.  Instead, post a task to
  // do all necessary infobar manipulation as soon as this call stack returns.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&InfoBarObserver::VerifyInfoBarState,
                            base::Unretained(this)));
}

void PPAPITestBase::InfoBarObserver::VerifyInfoBarState() {
  content::WebContents* web_contents =
      test_base_->browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents != NULL);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  ASSERT_TRUE(infobar_service != NULL);

  EXPECT_EQ(expecting_infobar_ ? 1U : 0U, infobar_service->infobar_count());
  if (!expecting_infobar_)
    return;
  expecting_infobar_ = false;

  infobars::InfoBar* infobar = infobar_service->infobar_at(0);
  ConfirmInfoBarDelegate* delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate != NULL);
  if (should_accept_)
    delegate->Accept();
  else
    delegate->Cancel();

  infobar_service->RemoveInfoBar(infobar);
}

PPAPITestBase::PPAPITestBase() {
}

void PPAPITestBase::SetUp() {
  EnablePixelOutput();
  InProcessBrowserTest::SetUp();
}

void PPAPITestBase::SetUpCommandLine(base::CommandLine* command_line) {
  // The test sends us the result via a cookie.
  command_line->AppendSwitch(switches::kEnableFileCookies);

  // Some stuff is hung off of the testing interface which is not enabled
  // by default.
  command_line->AppendSwitch(switches::kEnablePepperTesting);

  // Smooth scrolling confuses the scrollbar test.
  command_line->AppendSwitch(switches::kDisableSmoothScrolling);
}

void PPAPITestBase::SetUpOnMainThread() {
  // Always allow access to the PPAPI broker.
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PPAPI_BROKER, CONTENT_SETTING_ALLOW);
}

GURL PPAPITestBase::GetTestFileUrl(const std::string& test_case) {
  base::FilePath test_path;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &test_path));
  test_path = test_path.Append(FILE_PATH_LITERAL("ppapi"));
  test_path = test_path.Append(FILE_PATH_LITERAL("tests"));
  test_path = test_path.Append(FILE_PATH_LITERAL("test_case.html"));

  // Sanity check the file name.
  EXPECT_TRUE(base::PathExists(test_path));

  GURL test_url = net::FilePathToFileURL(test_path);

  GURL::Replacements replacements;
  std::string query = BuildQuery(std::string(), test_case);
  replacements.SetQuery(query.c_str(), url::Component(0, query.size()));
  return test_url.ReplaceComponents(replacements);
}

void PPAPITestBase::RunTest(const std::string& test_case) {
  GURL url = GetTestFileUrl(test_case);
  RunTestURL(url);
}

void PPAPITestBase::RunTestViaHTTP(const std::string& test_case) {
  base::FilePath document_root;
  ASSERT_TRUE(ui_test_utils::GetRelativeBuildDirectory(&document_root));
  base::FilePath http_document_root;
  ASSERT_TRUE(ui_test_utils::GetRelativeBuildDirectory(&http_document_root));
  net::SpawnedTestServer http_server(net::SpawnedTestServer::TYPE_HTTP,
                                     net::SpawnedTestServer::kLocalhost,
                                     document_root);
  ASSERT_TRUE(http_server.Start());
  RunTestURL(GetTestURL(http_server, test_case, std::string()));
}

void PPAPITestBase::RunTestWithSSLServer(const std::string& test_case) {
  base::FilePath http_document_root;
  ASSERT_TRUE(ui_test_utils::GetRelativeBuildDirectory(&http_document_root));
  net::SpawnedTestServer http_server(net::SpawnedTestServer::TYPE_HTTP,
                                     net::SpawnedTestServer::kLocalhost,
                                     http_document_root);
  net::SpawnedTestServer ssl_server(net::SpawnedTestServer::TYPE_HTTPS,
                                    net::BaseTestServer::SSLOptions(),
                                    http_document_root);
  // Start the servers in parallel.
  ASSERT_TRUE(http_server.StartInBackground());
  ASSERT_TRUE(ssl_server.StartInBackground());
  // Wait until they are both finished before continuing.
  ASSERT_TRUE(http_server.BlockUntilStarted());
  ASSERT_TRUE(ssl_server.BlockUntilStarted());

  uint16_t port = ssl_server.host_port_pair().port();
  RunTestURL(GetTestURL(http_server,
                        test_case,
                        base::StringPrintf("ssl_server_port=%d", port)));
}

void PPAPITestBase::RunTestWithWebSocketServer(const std::string& test_case) {
  base::FilePath http_document_root;
  ASSERT_TRUE(ui_test_utils::GetRelativeBuildDirectory(&http_document_root));
  net::SpawnedTestServer http_server(net::SpawnedTestServer::TYPE_HTTP,
                                     net::SpawnedTestServer::kLocalhost,
                                     http_document_root);
  net::SpawnedTestServer ws_server(net::SpawnedTestServer::TYPE_WS,
                                   net::SpawnedTestServer::kLocalhost,
                                   net::GetWebSocketTestDataDirectory());
  // Start the servers in parallel.
  ASSERT_TRUE(http_server.StartInBackground());
  ASSERT_TRUE(ws_server.StartInBackground());
  // Wait until they are both finished before continuing.
  ASSERT_TRUE(http_server.BlockUntilStarted());
  ASSERT_TRUE(ws_server.BlockUntilStarted());

  std::string host = ws_server.host_port_pair().HostForURL();
  uint16_t port = ws_server.host_port_pair().port();
  RunTestURL(GetTestURL(http_server,
                        test_case,
                        base::StringPrintf(
                            "websocket_host=%s&websocket_port=%d",
                            host.c_str(),
                            port)));
}

void PPAPITestBase::RunTestIfAudioOutputAvailable(
    const std::string& test_case) {
  RunTest(test_case);
}

void PPAPITestBase::RunTestViaHTTPIfAudioOutputAvailable(
    const std::string& test_case) {
  RunTestViaHTTP(test_case);
}

void PPAPITestBase::RunTestURL(const GURL& test_url) {
#if defined(OS_WIN) && defined(USE_ASH)
  // PPAPITests are broken in Ash browser tests (http://crbug.com/263548).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests)) {
    LOG(WARNING) << "PPAPITests are disabled for Ash browser tests.";
    return;
  }
#endif

  // See comment above TestingInstance in ppapi/test/testing_instance.h.
  // Basically it sends messages using the DOM automation controller. The
  // value of "..." means it's still working and we should continue to wait,
  // any other value indicates completion (in this case it will start with
  // "PASS" or "FAIL"). This keeps us from timing out on waits for long tests.
  PPAPITestMessageHandler handler;
  content::JavascriptTestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      &handler);

  ui_test_utils::NavigateToURL(browser(), test_url);

  ASSERT_TRUE(observer.Run()) << handler.error_message();
  EXPECT_STREQ("PASS", handler.message().c_str());
}

GURL PPAPITestBase::GetTestURL(
    const net::SpawnedTestServer& http_server,
    const std::string& test_case,
    const std::string& extra_params) {
  std::string query = BuildQuery("files/test_case.html?", test_case);
  if (!extra_params.empty())
    query = base::StringPrintf("%s&%s", query.c_str(), extra_params.c_str());

  return http_server.GetURL(query);
}

PPAPITest::PPAPITest() : in_process_(true) {
}

void PPAPITest::SetUpCommandLine(base::CommandLine* command_line) {
  PPAPITestBase::SetUpCommandLine(command_line);

  // Append the switch to register the pepper plugin.
  // library name = <out dir>/<test_name>.<library_extension>
  // MIME type = application/x-ppapi-<test_name>
  base::FilePath plugin_dir;
  EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));

  base::FilePath plugin_lib = plugin_dir.Append(ppapi::GetTestLibraryName());
  EXPECT_TRUE(base::PathExists(plugin_lib));
  base::FilePath::StringType pepper_plugin = plugin_lib.value();
  pepper_plugin.append(FILE_PATH_LITERAL(";application/x-ppapi-tests"));
  command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                   pepper_plugin);
  command_line->AppendSwitchASCII(switches::kAllowNaClSocketAPI, "127.0.0.1");

  if (in_process_)
    command_line->AppendSwitch(switches::kPpapiInProcess);
}

std::string PPAPITest::BuildQuery(const std::string& base,
                                  const std::string& test_case){
  return base::StringPrintf("%stestcase=%s", base.c_str(), test_case.c_str());
}

void PPAPIPrivateTest::SetUpCommandLine(base::CommandLine* command_line) {
  PPAPITest::SetUpCommandLine(command_line);
  AddPrivateSwitches(command_line);
}

OutOfProcessPPAPITest::OutOfProcessPPAPITest() {
  in_process_ = false;
}

void OutOfProcessPPAPITest::SetUpCommandLine(base::CommandLine* command_line) {
  PPAPITest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
}

void OutOfProcessPPAPIPrivateTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  OutOfProcessPPAPITest::SetUpCommandLine(command_line);
  AddPrivateSwitches(command_line);
}

void PPAPINaClTest::SetUpCommandLine(base::CommandLine* command_line) {
#if !defined(DISABLE_NACL)
  PPAPITestBase::SetUpCommandLine(command_line);

  // Enable running (non-portable) NaCl outside of the Chrome web store.
  command_line->AppendSwitch(switches::kEnableNaCl);
  command_line->AppendSwitchASCII(switches::kAllowNaClSocketAPI, "127.0.0.1");
  command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
#endif
}

void PPAPINaClTest::SetUpOnMainThread() {
  base::FilePath plugin_lib;
  EXPECT_TRUE(PathService::Get(chrome::FILE_NACL_PLUGIN, &plugin_lib));
  EXPECT_TRUE(base::PathExists(plugin_lib));
}

void PPAPINaClTest::RunTest(const std::string& test_case) {
#if !defined(DISABLE_NACL)
  PPAPITestBase::RunTest(test_case);
#endif
}

void PPAPINaClTest::RunTestViaHTTP(const std::string& test_case) {
#if !defined(DISABLE_NACL)
  PPAPITestBase::RunTestViaHTTP(test_case);
#endif
}

void PPAPINaClTest::RunTestWithSSLServer(const std::string& test_case) {
#if !defined(DISABLE_NACL)
  PPAPITestBase::RunTestWithSSLServer(test_case);
#endif
}

void PPAPINaClTest::RunTestWithWebSocketServer(const std::string& test_case) {
#if !defined(DISABLE_NACL)
  PPAPITestBase::RunTestWithWebSocketServer(test_case);
#endif
}

void PPAPINaClTest::RunTestIfAudioOutputAvailable(
    const std::string& test_case) {
#if !defined(DISABLE_NACL)
  PPAPITestBase::RunTestIfAudioOutputAvailable(test_case);
#endif
}

void PPAPINaClTest::RunTestViaHTTPIfAudioOutputAvailable(
    const std::string& test_case) {
#if !defined(DISABLE_NACL)
  PPAPITestBase::RunTestViaHTTPIfAudioOutputAvailable(test_case);
#endif
}

// Append the correct mode and testcase string
std::string PPAPINaClNewlibTest::BuildQuery(const std::string& base,
                                            const std::string& test_case) {
  return base::StringPrintf("%smode=nacl_newlib&testcase=%s", base.c_str(),
                            test_case.c_str());
}

void PPAPIPrivateNaClNewlibTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PPAPINaClNewlibTest::SetUpCommandLine(command_line);
  AddPrivateSwitches(command_line);
}

// Append the correct mode and testcase string
std::string PPAPINaClGLibcTest::BuildQuery(const std::string& base,
                                           const std::string& test_case) {
  return base::StringPrintf("%smode=nacl_glibc&testcase=%s", base.c_str(),
                            test_case.c_str());
}

void PPAPIPrivateNaClGLibcTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PPAPINaClGLibcTest::SetUpCommandLine(command_line);
  AddPrivateSwitches(command_line);
}

// Append the correct mode and testcase string
std::string PPAPINaClPNaClTest::BuildQuery(const std::string& base,
                                           const std::string& test_case) {
  return base::StringPrintf("%smode=nacl_pnacl&testcase=%s", base.c_str(),
                            test_case.c_str());
}

void PPAPIPrivateNaClPNaClTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PPAPINaClPNaClTest::SetUpCommandLine(command_line);
  AddPrivateSwitches(command_line);
}

void PPAPINaClPNaClNonSfiTest::SetUpCommandLine(
    base::CommandLine* command_line) {
#if !defined(DISABLE_NACL)
  PPAPINaClTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kEnableNaClNonSfiMode);
#endif
}

std::string PPAPINaClPNaClNonSfiTest::BuildQuery(
    const std::string& base,
    const std::string& test_case) {
  return base::StringPrintf("%smode=nacl_pnacl_nonsfi&testcase=%s",
                            base.c_str(), test_case.c_str());
}

void PPAPIPrivateNaClPNaClNonSfiTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PPAPINaClPNaClNonSfiTest::SetUpCommandLine(command_line);
  AddPrivateSwitches(command_line);
}

void PPAPINaClTestDisallowedSockets::SetUpCommandLine(
    base::CommandLine* command_line) {
  PPAPITestBase::SetUpCommandLine(command_line);

  // Enable running (non-portable) NaCl outside of the Chrome web store.
  command_line->AppendSwitch(switches::kEnableNaCl);
}

// Append the correct mode and testcase string
std::string PPAPINaClTestDisallowedSockets::BuildQuery(
    const std::string& base,
    const std::string& test_case) {
  return base::StringPrintf("%smode=nacl_newlib&testcase=%s", base.c_str(),
                            test_case.c_str());
}

void PPAPIBrokerInfoBarTest::SetUpOnMainThread() {
  // The default content setting for the PPAPI broker is ASK. We purposefully
  // don't call PPAPITestBase::SetUpOnMainThread() to keep it that way.

  base::FilePath plugin_lib;
  EXPECT_TRUE(PathService::Get(chrome::FILE_NACL_PLUGIN, &plugin_lib));
  EXPECT_TRUE(base::PathExists(plugin_lib));
}
