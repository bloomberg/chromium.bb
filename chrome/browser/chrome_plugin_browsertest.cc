// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_util.h"
#include "webkit/plugins/webplugininfo.h"

using content::BrowserThread;

class ChromePluginTest : public InProcessBrowserTest {
 protected:
  ChromePluginTest() {}

  static GURL GetURL(const char* filename) {
    FilePath path;
    PathService::Get(content::DIR_TEST_DATA, &path);
    path = path.AppendASCII("plugin").AppendASCII(filename);
    CHECK(file_util::PathExists(path));
    return net::FilePathToFileURL(path);
  }

  static void LoadAndWait(Browser* window, const GURL& url, bool pass) {
    content::WebContents* web_contents = chrome::GetActiveWebContents(window);
    string16 expected_title(ASCIIToUTF16(pass ? "OK" : "plugin_not_found"));
    content::TitleWatcher title_watcher(web_contents, expected_title);
    title_watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));
    title_watcher.AlsoWaitForTitle(ASCIIToUTF16(
        pass ? "plugin_not_found" : "OK"));
    ui_test_utils::NavigateToURL(window, url);
    ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  static void CrashFlash() {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&CrashFlashInternal, runner->QuitClosure()));
    runner->Run();
  }

  static FilePath GetFlashPath() {
    FilePath path;
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    content::PluginService::GetInstance()->GetPlugins(
        base::Bind(&GetPluginsInfoCallback, &path, runner->QuitClosure()));
    runner->Run();
    return path;
  }

  static void EnableFlash(bool enable, Profile* profile) {
    FilePath flash_path = GetFlashPath();
    ASSERT_FALSE(flash_path.empty());

    PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile);
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    plugin_prefs->EnablePlugin(enable, flash_path, runner->QuitClosure());
    runner->Run();
  }

  static void EnsureFlashProcessCount(int expected) {
    int actual = 0;
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&CountPluginProcesses, &actual, runner->QuitClosure()));
    runner->Run();
    ASSERT_EQ(expected, actual);
  }

 private:
  static void CrashFlashInternal(const base::Closure& quit_task) {
    bool found = false;
    for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
      if (iter.GetData().type != content::PROCESS_TYPE_PLUGIN &&
          iter.GetData().type != content::PROCESS_TYPE_PPAPI_PLUGIN) {
        continue;
      }
      base::KillProcess(iter.GetData().handle, 0, true);
      found = true;
    }
    ASSERT_TRUE(found) << "Didn't find Flash process!";
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit_task);
  }

  static void GetPluginsInfoCallback(
      FilePath* flash_path,
      const base::Closure& quit_task,
      const std::vector<webkit::WebPluginInfo>& plugins) {
    for (std::vector<webkit::WebPluginInfo>::const_iterator it =
           plugins.begin(); it != plugins.end(); ++it) {
      if (it->name == ASCIIToUTF16("Shockwave Flash")) {
        *flash_path = it->path;
        break;
      }
    }
    quit_task.Run();
  }

  static void CountPluginProcesses(int* count, const base::Closure& quit_task) {
    for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
      if (iter.GetData().type == content::PROCESS_TYPE_PLUGIN ||
          iter.GetData().type == content::PROCESS_TYPE_PPAPI_PLUGIN) {
        (*count)++;
      }
    }
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit_task);
  }
};

// Tests a bunch of basic scenarios with Flash.
IN_PROC_BROWSER_TEST_F(ChromePluginTest, Flash) {
  // Official builds always have bundled Flash.
#if !defined(OFFICIAL_BUILD)
  if (GetFlashPath().empty()) {
    LOG(INFO) << "Test not running because couldn't find Flash.";
    return;
  }
#endif

  GURL url = GetURL("flash.html");
  EnsureFlashProcessCount(0);

  // Try a single tab.
  ASSERT_NO_FATAL_FAILURE(LoadAndWait(browser(), url, true));
  EnsureFlashProcessCount(1);
  Profile* profile = browser()->profile();
  // Try another tab.
  ASSERT_NO_FATAL_FAILURE(LoadAndWait(CreateBrowser(profile), url, true));
  // Try an incognito window.
  ASSERT_NO_FATAL_FAILURE(LoadAndWait(CreateIncognitoBrowser(), url, true));
  EnsureFlashProcessCount(1);

  // Now kill Flash process and verify it reloads.
  CrashFlash();
  EnsureFlashProcessCount(0);

  ASSERT_NO_FATAL_FAILURE(LoadAndWait(browser(), url, true));
  EnsureFlashProcessCount(1);

  // Now try disabling it.
  EnableFlash(false, profile);
  CrashFlash();

  ASSERT_NO_FATAL_FAILURE(LoadAndWait(browser(), url, false));
  EnsureFlashProcessCount(0);

  // Now enable it again.
  EnableFlash(true, profile);
  ASSERT_NO_FATAL_FAILURE(LoadAndWait(browser(), url, true));
  EnsureFlashProcessCount(1);
}
