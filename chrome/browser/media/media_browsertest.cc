// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_browsertest.h"

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

// Common test results.
const char MediaBrowserTest::kEnded[] = "ENDED";
const char MediaBrowserTest::kError[] = "ERROR";
const char MediaBrowserTest::kFailed[] = "FAILED";
const char MediaBrowserTest::kPluginCrashed[] = "PLUGIN_CRASHED";

MediaBrowserTest::MediaBrowserTest() : ignore_plugin_crash_(false) {
}

MediaBrowserTest::~MediaBrowserTest() {
}

void MediaBrowserTest::RunMediaTestPage(
    const std::string& html_page, std::vector<StringPair>* query_params,
    const std::string& expected_title, bool http) {
  GURL gurl;
  std::string query = "";
  if (query_params != NULL && !query_params->empty()) {
    std::vector<StringPair>::const_iterator itr = query_params->begin();
    query = itr->first + "=" + itr->second;
    ++itr;
    for (; itr != query_params->end(); ++itr) {
      query.append("&" + itr->first + "=" + itr->second);
    }
  }
  if (http) {
    ASSERT_TRUE(test_server()->Start());
    gurl = test_server()->GetURL("files/media/" + html_page + "?" + query);
  } else {
    base::FilePath test_file_path;
    PathService::Get(chrome::DIR_TEST_DATA, &test_file_path);
    test_file_path = test_file_path.AppendASCII("media")
                                   .AppendASCII(html_page);
    gurl = content::GetFileUrlWithQuery(test_file_path, query);
  }

  base::string16 final_title = RunTest(gurl, expected_title);
  EXPECT_EQ(base::ASCIIToUTF16(expected_title), final_title);
}

base::string16 MediaBrowserTest::RunTest(const GURL& gurl,
                                         const std::string& expected_title) {
  VLOG(0) << "Running test URL: " << gurl;
  // Observe the web contents for plugin crashes.
  Observe(browser()->tab_strip_model()->GetActiveWebContents());
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::ASCIIToUTF16(expected_title));
  AddWaitForTitles(&title_watcher);
  ui_test_utils::NavigateToURL(browser(), gurl);

  return title_watcher.WaitAndGetTitle();
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

