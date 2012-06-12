// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCRIPT_BADGE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_SCRIPT_BADGE_CONTROLLER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/browser/extensions/script_executor_impl.h"
#include "content/public/browser/web_contents_observer.h"

class ExtensionAction;
class ExtensionService;
class TabContents;

namespace IPC {
class Message;
}

namespace extensions {

class Extension;

// A LocationBarController which displays icons whenever a script is executing
// in a tab. It accomplishes this two different ways:
//
// - For content_script declarations, this receives IPCs from the renderer
//   notifying that a content script is running (either on this tab or one of
//   its frames), which is recorded.
// - The ScriptExecutor interface is exposed for programmatically executing
//   scripts. Successfully executed scripts are recorded.
//
// When extension IDs are recorded a NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED
// is sent, and those extensions will be returned from GetCurrentActions until
// the next page navigation.
//
// Ref-counted so that it can be safely bound in a base::Bind.
class ScriptBadgeController
    : public base::RefCountedThreadSafe<ScriptBadgeController>,
      public LocationBarController,
      public ScriptExecutor,
      public content::WebContentsObserver {
 public:
  explicit ScriptBadgeController(TabContents* tab_contents);

  // LocationBarController implementation.
  virtual scoped_ptr<std::vector<ExtensionAction*> > GetCurrentActions()
      OVERRIDE;
  virtual Action OnClicked(const std::string& extension_id,
                           int mouse_button) OVERRIDE;

  // ScriptExecutor implementation.
  virtual void ExecuteScript(const std::string& extension_id,
                             ScriptType script_type,
                             const std::string& code,
                             FrameScope frame_scope,
                             UserScript::RunLocation run_at,
                             WorldType world_type,
                             const ExecuteScriptCallback& callback) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<ScriptBadgeController>;
  virtual ~ScriptBadgeController();

  // Callback for ExecuteScript which if successful and for the current URL
  // records that the script is running, then calls the original callback.
  void OnExecuteScriptFinished(const std::string& extension_id,
                               const ExecuteScriptCallback& callback,
                               bool success,
                               int32 page_id,
                               const std::string& error);

  // Gets the ExtensionService for |tab_contents_|.
  ExtensionService* GetExtensionService();

  // Gets the current page ID.
  int32 GetPageID();

  // Sends a LOCATION_BAR_UPDATED notification.
  void Notify();

  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Message handlers.
  void OnContentScriptsExecuting(const std::set<std::string>& extension_ids,
                                 int32 page_id);

  // Delegate ScriptExecutorImpl for running ExecuteScript.
  ScriptExecutorImpl script_executor_;

  // Our parent TabContents.
  TabContents* tab_contents_;

  // The extensions that have called ExecuteScript on the current frame.
  std::set<std::string> extensions_executing_scripts_;

  DISALLOW_COPY_AND_ASSIGN(ScriptBadgeController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCRIPT_BADGE_CONTROLLER_H_
