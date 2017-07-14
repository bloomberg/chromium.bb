// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_integration_browsertest.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_impl.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

namespace media_router {

namespace {
// Command line argument to specify receiver,
const char kReceiver[] = "receiver";
// The path relative to <chromium src>/out/<build config> for media router
// browser test resources.
const base::FilePath::StringPieceType kResourcePath = FILE_PATH_LITERAL(
    "media_router/browser_test_resources/");
const char kTestSinkName[] = "test-sink-1";
// The javascript snippets.
const char kCheckSessionScript[] = "checkSession();";
const char kCheckStartFailedScript[] = "checkStartFailed('%s', '%s');";
const char kStartSessionScript[] = "startSession();";
const char kTerminateSessionScript[] =
    "terminateSessionAndWaitForStateChange();";
const char kCloseSessionScript[] = "closeConnectionAndWaitForStateChange();";
const char kReconnectSessionScript[] = "reconnectSession('%s');";
const char kCheckSendMessageFailedScript[] = "checkSendMessageFailed('%s');";
const char kWaitSinkScript[] = "waitUntilDeviceAvailable();";
const char kSendMessageAndExpectResponseScript[] =
    "sendMessageAndExpectResponse('%s');";
const char kSendMessageAndExpectConnectionCloseOnErrorScript[] =
    "sendMessageAndExpectConnectionCloseOnError()";
const char kChooseSinkScript[] =
    "var sinks = Array.from(document.getElementById('media-router-container')."
    "  shadowRoot.getElementById('sink-list').getElementsByTagName('span'));"
    "var sink = sinks.find(sink => sink.textContent.trim() == '%s');"
    "if (sink) {"
    "  sink.click();"
    "}";
const char kCloseRouteScript[] =
    "window.document.getElementById('media-router-container').shadowRoot."
    "  getElementById('route-details').shadowRoot.getElementById("
    "    'close-route-button').click()";
const char kClickDialog[] =
    "window.document.getElementById('media-router-container').click();";
const char kGetSinkIdScript[] =
    "var sinks = window.document.getElementById('media-router-container')."
    "  allSinks;"
    "var sink = sinks.find(sink => sink.name == '%s');"
    "window.domAutomationController.send(sink ? sink.id : '');";
const char kGetRouteIdScript[] =
    "var routes = window.document.getElementById('media-router-container')."
    "  routeList;"
    "var route = routes.find(route => route.sinkId == '%s');"
    "window.domAutomationController.send(route ? route.id : '');";
const char kFindSinkScript[] =
    "var sinkList = document.getElementById('media-router-container')."
    "  shadowRoot.getElementById('sink-list');"
    "if (!sinkList) {"
    "  window.domAutomationController.send(false);"
    "} else {"
    "  var sinks = Array.from(sinkList.getElementsByTagName('span'));"
    "  var result = sinks.some(sink => sink.textContent.trim() == '%s');"
    "  window.domAutomationController.send(result);"
    "}";
const char kCheckDialogLoadedScript[] =
    "var container = document.getElementById('media-router-container');"
    "/** Wait until media router container is not undefined and "
    "*   deviceMissingUrl is not undefined, "
    "*   once deviceMissingUrl is not undefined, which means "
    "*   the dialog is fully loaded."
    "*/"
    "window.domAutomationController.send(!!container && "
    "    !!container.deviceMissingUrl);";

std::string GetStartedConnectionId(WebContents* web_contents) {
  std::string session_id;
  CHECK(content::ExecuteScriptAndExtractString(
      web_contents, "window.domAutomationController.send(startedConnection.id)",
      &session_id));
  return session_id;
}

std::string GetDefaultRequestSessionId(WebContents* web_contents) {
  std::string session_id;
  CHECK(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(defaultRequestSessionId)",
      &session_id));
  return session_id;
}

}  // namespace

MediaRouterIntegrationBrowserTest::MediaRouterIntegrationBrowserTest() {
}

MediaRouterIntegrationBrowserTest::~MediaRouterIntegrationBrowserTest() {
}

void MediaRouterIntegrationBrowserTest::TearDownOnMainThread() {
  MediaRouterBaseBrowserTest::TearDownOnMainThread();
  test_navigation_observer_.reset();
}

void MediaRouterIntegrationBrowserTest::ExecuteJavaScriptAPI(
    WebContents* web_contents,
    const std::string& script) {
  std::string result(ExecuteScriptAndExtractString(web_contents, script));

  // Read the test result, the test result set by javascript is a
  // JSON string with the following format:
  // {"passed": "<true/false>", "errorMessage": "<error_message>"}
  std::unique_ptr<base::Value> value =
      base::JSONReader::Read(result, base::JSON_ALLOW_TRAILING_COMMAS);

  // Convert to dictionary.
  base::DictionaryValue* dict_value = nullptr;
  ASSERT_TRUE(value->GetAsDictionary(&dict_value));

  // Extract the fields.
  bool passed = false;
  ASSERT_TRUE(dict_value->GetBoolean("passed", &passed));
  std::string error_message;
  ASSERT_TRUE(dict_value->GetString("errorMessage", &error_message));

  ASSERT_TRUE(passed) << error_message;
}

void MediaRouterIntegrationBrowserTest::StartSessionAndAssertNotFoundError() {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  WebContents* web_contents = GetActiveWebContents();
  CHECK(web_contents);
  StartSession(web_contents);

  // Wait for any sinks to be displayed.
  Wait(base::TimeDelta::FromSeconds(1));
  GetControllerForShownDialog(web_contents)->HideMediaRouterDialog();
  CheckStartFailed(web_contents, "NotFoundError", "No screens found.");
}

WebContents*
MediaRouterIntegrationBrowserTest::StartSessionWithTestPageAndSink() {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  WebContents* web_contents = GetActiveWebContents();
  CHECK(web_contents);
  ExecuteJavaScriptAPI(web_contents, kWaitSinkScript);
  StartSession(web_contents);
  return web_contents;
}

WebContents*
MediaRouterIntegrationBrowserTest::StartSessionWithTestPageAndChooseSink() {
  WebContents* web_contents = StartSessionWithTestPageAndSink();
  WaitUntilSinkDiscoveredOnUI();
  ChooseSink(web_contents, receiver_);
  return web_contents;
}

void MediaRouterIntegrationBrowserTest::OpenTestPage(
    base::FilePath::StringPieceType file_name) {
  base::FilePath full_path = GetResourceFile(file_name);
  ui_test_utils::NavigateToURL(browser(), GetTestPageUrl(full_path));
}

void MediaRouterIntegrationBrowserTest::OpenTestPageInNewTab(
    base::FilePath::StringPieceType file_name) {
  base::FilePath full_path = GetResourceFile(file_name);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestPageUrl(full_path),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
}

GURL MediaRouterIntegrationBrowserTest::GetTestPageUrl(
    const base::FilePath& full_path) {
  return net::FilePathToFileURL(full_path);
}

void MediaRouterIntegrationBrowserTest::StartSession(
    WebContents* web_contents) {
  test_navigation_observer_.reset(
      new content::TestNavigationObserver(web_contents, 1));
  test_navigation_observer_->StartWatchingNewWebContents();
  ExecuteJavaScriptAPI(web_contents, kStartSessionScript);
  test_navigation_observer_->Wait();
  test_navigation_observer_->StopWatchingNewWebContents();
}

void MediaRouterIntegrationBrowserTest::ChooseSink(
    WebContents* web_contents,
    const std::string& sink_name) {
  WebContents* dialog_contents = GetMRDialog(web_contents);
  std::string script = base::StringPrintf(
      kChooseSinkScript, sink_name.c_str());
  // Execute javascript to choose sink, but don't wait until it finishes.
  dialog_contents->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::UTF8ToUTF16(script));
}

void MediaRouterIntegrationBrowserTest::CheckStartFailed(
    WebContents* web_contents,
    const std::string& error_name,
    const std::string& error_message_substring) {
  std::string script(base::StringPrintf(kCheckStartFailedScript,
                                        error_name.c_str(),
                                        error_message_substring.c_str()));
  ExecuteJavaScriptAPI(web_contents, script);
}

void MediaRouterIntegrationBrowserTest::ClickDialog() {
  WebContents* web_contents = GetActiveWebContents();
  WebContents* dialog_contents = GetMRDialog(web_contents);
  ASSERT_TRUE(content::ExecuteScript(dialog_contents, kClickDialog));
}
WebContents* MediaRouterIntegrationBrowserTest::GetMRDialog(
    WebContents* web_contents) {
  MediaRouterDialogControllerImpl* controller =
      MediaRouterDialogControllerImpl::GetOrCreateForWebContents(web_contents);
  WebContents* dialog_contents = controller->GetMediaRouterDialog();
  CHECK(dialog_contents);
  WaitUntilDialogFullyLoaded(dialog_contents);
  return dialog_contents;
}

bool MediaRouterIntegrationBrowserTest::IsDialogClosed(
    WebContents* web_contents) {
  MediaRouterDialogControllerImpl* controller =
      MediaRouterDialogControllerImpl::GetOrCreateForWebContents(web_contents);
  return !controller->GetMediaRouterDialog();
}

void MediaRouterIntegrationBrowserTest::WaitUntilDialogClosed(
    WebContents* web_contents) {
  ASSERT_TRUE(ConditionalWait(
      base::TimeDelta::FromSeconds(5), base::TimeDelta::FromSeconds(1),
      base::Bind(&MediaRouterIntegrationBrowserTest::IsDialogClosed,
                 base::Unretained(this), web_contents)));
}

void MediaRouterIntegrationBrowserTest::CheckDialogRemainsOpen(
    WebContents* web_contents) {
  ASSERT_FALSE(ConditionalWait(
      base::TimeDelta::FromSeconds(5), base::TimeDelta::FromSeconds(1),
      base::Bind(&MediaRouterIntegrationBrowserTest::IsDialogClosed,
                 base::Unretained(this), web_contents)));
}

void MediaRouterIntegrationBrowserTest::SetTestData(
    base::FilePath::StringPieceType test_data_file) {
  base::FilePath full_path = GetResourceFile(test_data_file);
  JSONFileValueDeserializer deserializer(full_path);
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<base::Value> value;
  {
    // crbug.com/724573
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    value = deserializer.Deserialize(&error_code, &error_message);
  }
  CHECK(value.get()) << "Deserialize failed: " << error_message;
  std::string test_data_str;
  ASSERT_TRUE(base::JSONWriter::Write(*value, &test_data_str));
  ExecuteScriptInBackgroundPageNoWait(
      extension_id_,
      base::StringPrintf("localStorage['testdata'] = '%s'",
                         test_data_str.c_str()));
}

WebContents* MediaRouterIntegrationBrowserTest::OpenMRDialog(
    WebContents* web_contents) {
  MediaRouterDialogControllerImpl* controller =
      MediaRouterDialogControllerImpl::GetOrCreateForWebContents(web_contents);
  test_navigation_observer_.reset(
        new content::TestNavigationObserver(web_contents, 1));
  test_navigation_observer_->StartWatchingNewWebContents();
  CHECK(controller->ShowMediaRouterDialog());
  test_navigation_observer_->Wait();
  test_navigation_observer_->StopWatchingNewWebContents();
  WebContents* dialog_contents = controller->GetMediaRouterDialog();
  CHECK(dialog_contents);
  WaitUntilDialogFullyLoaded(dialog_contents);
  return dialog_contents;
}

base::FilePath MediaRouterIntegrationBrowserTest::GetResourceFile(
    base::FilePath::StringPieceType relative_path) const {
  base::FilePath base_dir;
  // ASSERT_TRUE can only be used in void returning functions.
  // Use CHECK instead in non-void returning functions.
  CHECK(PathService::Get(base::DIR_MODULE, &base_dir));
  base::FilePath full_path =
      base_dir.Append(kResourcePath).Append(relative_path);
  {
    // crbug.com/724573
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    CHECK(PathExists(full_path));
  }
  return full_path;
}

int MediaRouterIntegrationBrowserTest::ExecuteScriptAndExtractInt(
    const content::ToRenderFrameHost& adapter, const std::string& script) {
  int result;
  CHECK(content::ExecuteScriptAndExtractInt(adapter, script, &result));
  return result;
}

std::string MediaRouterIntegrationBrowserTest::ExecuteScriptAndExtractString(
    const content::ToRenderFrameHost& adapter, const std::string& script) {
  std::string result;
  CHECK(content::ExecuteScriptAndExtractString(adapter, script, &result));
  return result;
}

bool MediaRouterIntegrationBrowserTest::ExecuteScriptAndExtractBool(
    const content::ToRenderFrameHost& adapter, const std::string& script) {
  bool result;
  CHECK(content::ExecuteScriptAndExtractBool(adapter, script, &result));
  return result;
}

void MediaRouterIntegrationBrowserTest::ExecuteScript(
    const content::ToRenderFrameHost& adapter, const std::string& script) {
  ASSERT_TRUE(content::ExecuteScript(adapter, script));
}

bool MediaRouterIntegrationBrowserTest::IsRouteCreatedOnUI() {
  return !GetRouteId(receiver()).empty();
}

std::string MediaRouterIntegrationBrowserTest::GetRouteId(
    const std::string& sink_name) {
  WebContents* web_contents = GetActiveWebContents();
  WebContents* dialog_contents = GetMRDialog(web_contents);
  std::string script = base::StringPrintf(kGetSinkIdScript, sink_name.c_str());
  std::string sink_id = ExecuteScriptAndExtractString(dialog_contents, script);
  DVLOG(0) << "sink id: " << sink_id;
  script = base::StringPrintf(kGetRouteIdScript, sink_id.c_str());
  std::string route_id = ExecuteScriptAndExtractString(dialog_contents, script);
  DVLOG(0) << "route id: " << route_id;
  return route_id;
}

void MediaRouterIntegrationBrowserTest::WaitUntilRouteCreated() {
  ASSERT_TRUE(ConditionalWait(
      base::TimeDelta::FromSeconds(10), base::TimeDelta::FromSeconds(1),
      base::Bind(&MediaRouterIntegrationBrowserTest::IsRouteCreatedOnUI,
                 base::Unretained(this))));
}

bool MediaRouterIntegrationBrowserTest::IsUIShowingIssue() {
  WebContents* web_contents = GetActiveWebContents();
  WebContents* dialog_contents = GetMRDialog(web_contents);
  std::string script = base::StringPrintf(
      "domAutomationController.send(window.document.getElementById("
      "'media-router-container').issue != undefined)");
  bool has_issue = false;
  CHECK(content::ExecuteScriptAndExtractBool(dialog_contents, script,
                                             &has_issue));
  return has_issue;
}

void MediaRouterIntegrationBrowserTest::WaitUntilIssue() {
  ASSERT_TRUE(ConditionalWait(
      base::TimeDelta::FromSeconds(30), base::TimeDelta::FromSeconds(1),
      base::Bind(&MediaRouterIntegrationBrowserTest::IsUIShowingIssue,
                 base::Unretained(this))));
}

std::string MediaRouterIntegrationBrowserTest::GetIssueTitle() {
  WebContents* web_contents = GetActiveWebContents();
  WebContents* dialog_contents = GetMRDialog(web_contents);
  std::string script = base::StringPrintf(
      "domAutomationController.send(window.document.getElementById("
      "'media-router-container').issue.title)");
  return ExecuteScriptAndExtractString(dialog_contents, script);
}

bool MediaRouterIntegrationBrowserTest::IsRouteClosedOnUI() {
  // After execute js script to close route on UI, the dialog will dispear
  // after 3s. But sometimes it takes more than 3s to close the route, so
  // we need to re-open the dialog if it is closed.
  WebContents* web_contents = GetActiveWebContents();
  MediaRouterDialogControllerImpl* controller =
      MediaRouterDialogControllerImpl::GetOrCreateForWebContents(web_contents);
  WebContents* dialog_contents = controller->GetMediaRouterDialog();
  if (!dialog_contents) {
    VLOG(0) << "Media router dialog was closed, reopen it again.";
    OpenMRDialog(web_contents);
  }
  return GetRouteId(receiver()).empty();
}

void MediaRouterIntegrationBrowserTest::CloseRouteOnUI() {
  WebContents* web_contents = GetActiveWebContents();
  WebContents* dialog_contents = GetMRDialog(web_contents);
  ASSERT_TRUE(content::ExecuteScript(dialog_contents, kCloseRouteScript));
  ASSERT_TRUE(ConditionalWait(
      base::TimeDelta::FromSeconds(10), base::TimeDelta::FromSeconds(1),
      base::Bind(&MediaRouterIntegrationBrowserTest::IsRouteClosedOnUI,
                 base::Unretained(this))));
}

bool MediaRouterIntegrationBrowserTest::IsSinkDiscoveredOnUI() {
  WebContents* web_contents = GetActiveWebContents();
  WebContents* dialog_contents = GetMRDialog(web_contents);
  std::string script = base::StringPrintf(kFindSinkScript, receiver().c_str());
  return ExecuteScriptAndExtractBool(dialog_contents, script);
}

void MediaRouterIntegrationBrowserTest::WaitUntilSinkDiscoveredOnUI() {
  DVLOG(0) << "Receiver name: " << receiver_;
  // Wait for sink to show up in UI.
  ASSERT_TRUE(ConditionalWait(
      base::TimeDelta::FromSeconds(30), base::TimeDelta::FromSeconds(1),
      base::Bind(&MediaRouterIntegrationBrowserTest::IsSinkDiscoveredOnUI,
                 base::Unretained(this))));
}

bool MediaRouterIntegrationBrowserTest::IsDialogLoaded(
    WebContents* dialog_contents) {
  return ExecuteScriptAndExtractBool(dialog_contents, kCheckDialogLoadedScript);
}

void MediaRouterIntegrationBrowserTest::WaitUntilDialogFullyLoaded(
    WebContents* dialog_contents) {
  ASSERT_TRUE(ConditionalWait(
      base::TimeDelta::FromSeconds(30), base::TimeDelta::FromSeconds(1),
      base::Bind(&MediaRouterIntegrationBrowserTest::IsDialogLoaded,
                 base::Unretained(this), dialog_contents)));
}

void MediaRouterIntegrationBrowserTest::ParseCommandLine() {
  MediaRouterBaseBrowserTest::ParseCommandLine();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  receiver_ = command_line->GetSwitchValueASCII(kReceiver);
  if (receiver_.empty())
    receiver_ = kTestSinkName;
}

void MediaRouterIntegrationBrowserTest::CheckSessionValidity(
    WebContents* web_contents) {
  ExecuteJavaScriptAPI(web_contents, kCheckSessionScript);
  std::string session_id(GetStartedConnectionId(web_contents));
  EXPECT_FALSE(session_id.empty());
  std::string default_request_session_id(
      GetDefaultRequestSessionId(web_contents));
  EXPECT_EQ(session_id, default_request_session_id);
}

MediaRouterDialogControllerImpl*
MediaRouterIntegrationBrowserTest::GetControllerForShownDialog(
    WebContents* web_contents) {
  MediaRouterDialogControllerImpl* controller =
      MediaRouterDialogControllerImpl::GetOrCreateForWebContents(web_contents);
  EXPECT_TRUE(controller->IsShowingMediaRouterDialog());
  return controller;
}

WebContents* MediaRouterIntegrationBrowserTest::GetActiveWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void MediaRouterIntegrationBrowserTest::RunBasicTest() {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents, kTerminateSessionScript);
}

void MediaRouterIntegrationBrowserTest::RunSendMessageTest(
    const std::string& message) {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(
      web_contents,
      base::StringPrintf(kSendMessageAndExpectResponseScript, message.c_str()));
}

void MediaRouterIntegrationBrowserTest::RunFailToSendMessageTest() {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents, kCloseSessionScript);

  ExecuteJavaScriptAPI(
      web_contents,
      base::StringPrintf(kCheckSendMessageFailedScript, "closed"));
}

void MediaRouterIntegrationBrowserTest::RunReconnectSessionTest() {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  std::string session_id(GetStartedConnectionId(web_contents));

  // Wait a few seconds for MediaRouter to receive updates containing the
  // created route.
  Wait(base::TimeDelta::FromSeconds(3));

  OpenTestPageInNewTab(FILE_PATH_LITERAL("basic_test.html"));
  WebContents* new_web_contents = GetActiveWebContents();
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);
  ExecuteJavaScriptAPI(
      new_web_contents,
      base::StringPrintf(kReconnectSessionScript, session_id.c_str()));
  std::string reconnected_session_id;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      new_web_contents,
      "window.domAutomationController.send(reconnectedSession.id)",
      &reconnected_session_id));
  ASSERT_EQ(session_id, reconnected_session_id);
}

void MediaRouterIntegrationBrowserTest::RunReconnectSessionSameTabTest() {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  std::string session_id(GetStartedConnectionId(web_contents));
  ExecuteJavaScriptAPI(web_contents, kCloseSessionScript);

  ExecuteJavaScriptAPI(web_contents, base::StringPrintf(kReconnectSessionScript,
                                                        session_id.c_str()));
  std::string reconnected_session_id;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(reconnectedSession.id)",
      &reconnected_session_id));
  ASSERT_EQ(session_id, reconnected_session_id);
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, MANUAL_Basic) {
  RunBasicTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_SendAndOnMessage) {
  RunSendMessageTest("foo");
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, MANUAL_CloseOnError) {
  SetTestData(FILE_PATH_LITERAL("close_route_with_error_on_send.json"));
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents,
                       kSendMessageAndExpectConnectionCloseOnErrorScript);
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_SendMessage) {
  RunFailToSendMessageTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_NoProvider) {
  SetTestData(FILE_PATH_LITERAL("no_provider.json"));
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckStartFailed(web_contents, "UnknownError",
                   "No provider supports createRoute with source");
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_CreateRoute) {
  SetTestData(FILE_PATH_LITERAL("fail_create_route.json"));
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckStartFailed(web_contents, "UnknownError", "Unknown sink");
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_ReconnectSession) {
  RunReconnectSessionTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_ReconnectSession) {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  std::string session_id(GetStartedConnectionId(web_contents));

  // Wait a few seconds for MediaRouter to receive updates containing the
  // created route.
  Wait(base::TimeDelta::FromSeconds(3));

  SetTestData(FILE_PATH_LITERAL("fail_reconnect_session.json"));
  OpenTestPageInNewTab(FILE_PATH_LITERAL("fail_reconnect_session.html"));
  WebContents* new_web_contents = GetActiveWebContents();
  ASSERT_TRUE(new_web_contents);
  ExecuteJavaScriptAPI(
      new_web_contents,
      base::StringPrintf("checkReconnectSessionFails('%s');",
                         session_id.c_str()));
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_StartCancelled) {
  WebContents* web_contents = StartSessionWithTestPageAndSink();
  GetControllerForShownDialog(web_contents)->HideMediaRouterDialog();
  CheckStartFailed(web_contents, "NotAllowedError", "Dialog closed.");
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_StartCancelledNoSinks) {
  SetTestData(FILE_PATH_LITERAL("no_sinks.json"));
  StartSessionAndAssertNotFoundError();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_StartCancelledNoSupportedSinks) {
  SetTestData(FILE_PATH_LITERAL("no_supported_sinks.json"));
  StartSessionAndAssertNotFoundError();
}

void MediaRouterIntegrationIncognitoBrowserTest::InstallAndEnableMRExtension() {
  const extensions::Extension* extension =
      LoadExtensionIncognito(extension_unpacked_);
  incognito_extension_id_ = extension->id();
}

void MediaRouterIntegrationIncognitoBrowserTest::UninstallMRExtension() {
  if (!incognito_extension_id_.empty()) {
    UninstallExtension(incognito_extension_id_);
  }
}

Browser* MediaRouterIntegrationIncognitoBrowserTest::browser() {
  if (!incognito_browser_)
    incognito_browser_ = CreateIncognitoBrowser();
  return incognito_browser_;
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationIncognitoBrowserTest,
                       MANUAL_Basic) {
  RunBasicTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationIncognitoBrowserTest,
                       MANUAL_ReconnectSession) {
  RunReconnectSessionTest();
}

}  // namespace media_router
