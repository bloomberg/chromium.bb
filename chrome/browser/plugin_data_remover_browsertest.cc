// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_data_remover.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

namespace {
const char* kNPAPITestPluginMimeType = "application/vnd.npapi-test";
}

class PluginDataRemoverTest : public InProcessBrowserTest,
                              public base::WaitableEventWatcher::Delegate {
 public:
  PluginDataRemoverTest() : InProcessBrowserTest() { }

  virtual void OnWaitableEventSignaled(base::WaitableEvent* waitable_event) {
    MessageLoop::current()->Quit();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
#ifdef OS_MACOSX
    FilePath browser_directory;
    PathService::Get(chrome::DIR_APP, &browser_directory);
    command_line->AppendSwitchPath(switches::kExtraPluginDir,
                                   browser_directory.AppendASCII("plugins"));
#endif
  }
};

IN_PROC_BROWSER_TEST_F(PluginDataRemoverTest, RemoveData) {
  scoped_refptr<PluginDataRemover> plugin_data_remover(
      new PluginDataRemover(browser()->profile()));
  plugin_data_remover->set_mime_type(kNPAPITestPluginMimeType);
  base::WaitableEventWatcher watcher;
  base::WaitableEvent* event =
      plugin_data_remover->StartRemoving(base::Time());
  watcher.StartWatching(event, this);
  ui_test_utils::RunMessageLoop();
}
