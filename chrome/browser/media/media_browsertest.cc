// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_browsertest.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/test_data_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

// Common test results.
const char MediaBrowserTest::kEnded[] = "ENDED";
const char MediaBrowserTest::kError[] = "ERROR";
const char MediaBrowserTest::kFailed[] = "FAILED";
const char MediaBrowserTest::kPluginCrashed[] = "PLUGIN_CRASHED";

MediaBrowserTest::MediaBrowserTest() : ignore_plugin_crash_(false) {
}

MediaBrowserTest::~MediaBrowserTest() {
}

void MediaBrowserTest::RunMediaTestPage(const std::string& html_page,
                                        const media::QueryParams& query_params,
                                        const std::string& expected_title,
                                        bool http) {
  GURL gurl;
  std::string query = media::GetURLQueryString(query_params);
  scoped_ptr<net::SpawnedTestServer> http_test_server;
  if (http) {
    http_test_server.reset(
        new net::SpawnedTestServer(net::SpawnedTestServer::TYPE_HTTP,
                                   net::SpawnedTestServer::kLocalhost,
                                   media::GetTestDataPath()));
    CHECK(http_test_server->Start());
    gurl = http_test_server->GetURL("files/" + html_page + "?" + query);
  } else {
    gurl = content::GetFileUrlWithQuery(media::GetTestDataFilePath(html_page),
                                        query);
  }
  std::string final_title = RunTest(gurl, expected_title);
  EXPECT_EQ(expected_title, final_title);
}

std::string MediaBrowserTest::RunTest(const GURL& gurl,
                                      const std::string& expected_title) {
  DVLOG(0) << "Running test URL: " << gurl;
  // Observe the web contents for plugin crashes.
  Observe(browser()->tab_strip_model()->GetActiveWebContents());
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::ASCIIToUTF16(expected_title));
  AddWaitForTitles(&title_watcher);
  ui_test_utils::NavigateToURL(browser(), gurl);
  base::string16 result = title_watcher.WaitAndGetTitle();
  return base::UTF16ToASCII(result);
}

void MediaBrowserTest::AddWaitForTitles(content::TitleWatcher* title_watcher) {
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEnded));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kError));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kFailed));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kPluginCrashed));
}

void MediaBrowserTest::PluginCrashed(const base::FilePath& plugin_path,
                                     base::ProcessId plugin_pid) {
  VLOG(0) << "Plugin crashed: " << plugin_path.value();
  if (ignore_plugin_crash_)
    return;
  // Update document title to quit TitleWatcher early.
  web_contents()->GetController().GetActiveEntry()
      ->SetTitle(base::ASCIIToUTF16(kPluginCrashed));
  ADD_FAILURE() << "Failing test due to plugin crash.";
}

void MediaBrowserTest::IgnorePluginCrash() {
  ignore_plugin_crash_ = true;
}

