// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_integration_browsertest.h"

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_impl.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// The path relative to <chromium src>/out/<build config> for media router
// browser test resources.
const base::FilePath::StringPieceType kResourcePath = FILE_PATH_LITERAL(
    "media_router/browser_test_resources/");
}  // namespace


namespace media_router {

MediaRouterIntegrationBrowserTest::MediaRouterIntegrationBrowserTest() {
}

MediaRouterIntegrationBrowserTest::~MediaRouterIntegrationBrowserTest() {
}

void MediaRouterIntegrationBrowserTest::ExecuteJavaScriptAPI(
    content::WebContents* web_contents,
    const std::string& script) {
  std::string result;

  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(web_contents, script, &result));

  // Read the test result, the test result set by javascript is a
  // JSON string with the following format:
  // {"passed": "<true/false>", "errorMessage": "<error_message>"}
  scoped_ptr<base::Value> value =
      base::JSONReader::Read(result, base::JSON_ALLOW_TRAILING_COMMAS);

  // Convert to dictionary.
  base::DictionaryValue* dict_value = nullptr;
  ASSERT_TRUE(value->GetAsDictionary(&dict_value));

  // Extract the fields.
  bool passed = false;
  ASSERT_TRUE(dict_value->GetBoolean("passed", &passed));
  std::string error_message;
  ASSERT_TRUE(dict_value->GetString("errorMessage", &error_message));

  EXPECT_TRUE(passed) << error_message;
}

void MediaRouterIntegrationBrowserTest::OpenTestPage(
    base::FilePath::StringPieceType file_name) {
  base::FilePath full_path = GetResourceFile(file_name);
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(full_path));
}

void MediaRouterIntegrationBrowserTest::OpenTestPageInNewTab(
    base::FilePath::StringPieceType file_name) {
  base::FilePath full_path = GetResourceFile(file_name);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), net::FilePathToFileURL(full_path), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
}

void MediaRouterIntegrationBrowserTest::ChooseSink(
    content::WebContents* web_contents,
    const std::string& sink_id) {
  MediaRouterDialogControllerImpl* controller =
      MediaRouterDialogControllerImpl::GetOrCreateForWebContents(web_contents);
  content::WebContents* dialog_contents = controller->GetMediaRouterDialog();
  ASSERT_TRUE(dialog_contents);
  std::string script = base::StringPrintf(
      "window.document.getElementById('media-router-container')."
      "showOrCreateRoute_({'id': '%s', 'name': ''}, null)",
      sink_id.c_str());
  ASSERT_TRUE(content::ExecuteScript(dialog_contents, script));
}

void MediaRouterIntegrationBrowserTest::SetTestData(
    base::FilePath::StringPieceType test_data_file) {
  base::FilePath full_path = GetResourceFile(test_data_file);
  JSONFileValueDeserializer deserializer(full_path);
  int error_code = 0;
  std::string error_message;
  scoped_ptr<base::Value> value(
      deserializer.Deserialize(&error_code, &error_message));
  CHECK(value.get()) << "Deserialize failed: " << error_message;
  std::string test_data_str;
  ASSERT_TRUE(base::JSONWriter::Write(*value, &test_data_str));
  ExecuteScriptInBackgroundPageNoWait(
      extension_id_,
      base::StringPrintf("localStorage['testdata'] = '%s'",
                         test_data_str.c_str()));
}

base::FilePath MediaRouterIntegrationBrowserTest::GetResourceFile(
    base::FilePath::StringPieceType relative_path) const {
  base::FilePath base_dir;
  // ASSERT_TRUE can only be used in void returning functions.
  // Use CHECK instead in non-void returning functions.
  CHECK(PathService::Get(base::DIR_MODULE, &base_dir));
  base::FilePath full_path =
      base_dir.Append(kResourcePath).Append(relative_path);
  CHECK(PathExists(full_path));
  return full_path;
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, MANUAL_Basic) {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExecuteJavaScriptAPI(web_contents, "waitUntilDeviceAvailable();");
  content::TestNavigationObserver test_navigation_observer(web_contents, 1);
  test_navigation_observer.StartWatchingNewWebContents();
  ExecuteJavaScriptAPI(web_contents, "startSession();");
  test_navigation_observer.Wait();
  ChooseSink(web_contents, "id1");
  ExecuteJavaScriptAPI(web_contents, "checkSession();");
  Wait(base::TimeDelta::FromSeconds(5));
  ExecuteJavaScriptAPI(web_contents, "stopSession();");
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_No_Provider) {
  SetTestData(FILE_PATH_LITERAL("no_provider.json"));
  OpenTestPage(FILE_PATH_LITERAL("no_provider.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExecuteJavaScriptAPI(web_contents, "waitUntilDeviceAvailable();");
  content::TestNavigationObserver test_navigation_observer(web_contents, 1);
  test_navigation_observer.StartWatchingNewWebContents();
  ExecuteJavaScriptAPI(web_contents, "startSession();");
  test_navigation_observer.Wait();
  ChooseSink(web_contents, "id1");
  ExecuteJavaScriptAPI(web_contents, "checkSessionFailedToStart();");
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_Create_Route) {
  SetTestData(FILE_PATH_LITERAL("fail_create_route.json"));
  OpenTestPage(FILE_PATH_LITERAL("fail_create_route.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExecuteJavaScriptAPI(web_contents, "waitUntilDeviceAvailable();");
  content::TestNavigationObserver test_navigation_observer(web_contents, 1);
  test_navigation_observer.StartWatchingNewWebContents();
  ExecuteJavaScriptAPI(web_contents, "startSession();");
  test_navigation_observer.Wait();
  ChooseSink(web_contents, "id1");
  ExecuteJavaScriptAPI(web_contents, "checkSessionFailedToStart();");
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, MANUAL_JoinSession) {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExecuteJavaScriptAPI(web_contents, "waitUntilDeviceAvailable();");
  content::TestNavigationObserver test_navigation_observer(web_contents, 1);
  test_navigation_observer.StartWatchingNewWebContents();
  ExecuteJavaScriptAPI(web_contents, "startSession();");
  test_navigation_observer.Wait();
  ChooseSink(web_contents, "id1");
  ExecuteJavaScriptAPI(web_contents, "checkSession();");
  std::string session_id;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, "window.domAutomationController.send(startedSession.id)",
      &session_id));

  OpenTestPageInNewTab(FILE_PATH_LITERAL("basic_test.html"));
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);
  ExecuteJavaScriptAPI(
      new_web_contents,
      base::StringPrintf("joinSession('%s');", session_id.c_str()));
  std::string joined_session_id;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      new_web_contents, "window.domAutomationController.send(joinedSession.id)",
      &joined_session_id));
  ASSERT_EQ(session_id, joined_session_id);
}

}  // namespace media_router
