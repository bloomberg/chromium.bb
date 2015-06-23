// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_integration_browsertest.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  scoped_ptr<base::Value> value = base::JSONReader::Read(
      result, base::JSON_ALLOW_TRAILING_COMMAS);

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
  base::FilePath base_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_EXE, &base_dir));
  base::FilePath full_path = base_dir
      .Append(FILE_PATH_LITERAL("media_router/browser_test_resources/"))
      .Append(file_name);
  ASSERT_TRUE(PathExists(full_path));
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(full_path));
}

void MediaRouterIntegrationBrowserTest::ChooseSink(
    content::WebContents* web_contents,
    const std::string& sink_id) {
  MediaRouterDialogController* controller =
      MediaRouterDialogController::GetOrCreateForWebContents(web_contents);
  content::WebContents* dialog_contents = controller->GetMediaRouterDialog();
  ASSERT_TRUE(dialog_contents);
  std::string script = base::StringPrintf(
      "window.document.getElementById('media-router-container')."
      "showOrCreateRoute_({'id': '%s', 'name': ''}, null)",
      sink_id.c_str());
  ASSERT_TRUE(content::ExecuteScript(dialog_contents, script));
}

// TODO(leilei): Enable this test case once the buildbot is updated.
// To make it pass on buildbot, we need to run browser tests after download
// media router dev extension.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, MANUAL_Basic) {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::TestNavigationObserver test_navigation_observer(web_contents, 1);
  test_navigation_observer.StartWatchingNewWebContents();
  ExecuteJavaScriptAPI(web_contents, "startSession();");
  test_navigation_observer.Wait();
  ChooseSink(web_contents, "id1");
  ExecuteJavaScriptAPI(web_contents, "checkSession();");
  Wait(base::TimeDelta::FromSeconds(5));
  ExecuteJavaScriptAPI(web_contents, "stopSession();");
}

}  // namespace media_router
