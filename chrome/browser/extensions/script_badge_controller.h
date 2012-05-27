// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCRIPT_BADGE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_SCRIPT_BADGE_CONTROLLER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/browser/extensions/script_executor_impl.h"
#include "content/public/browser/web_contents_observer.h"

class ExtensionAction;
class ExtensionService;
class TabContentsWrapper;

namespace extensions {

class Extension;

// An LocationBarController which displays icons whenever a script is executing
// in a tab. It accomplishes this two different ways:
//
// - For content_script declarations, the current URL in the tab is compared to
//   registered content scripts when GetCurrentActions() is called.
// - An interface is exposed for programmatically executing scripts. Executed
//   scripts are recorded and used later to populate GetCurrentActions().
//
// TODO(aa): There are some edge cases that need to be thought-through here:
//
// - Redirects. In this case, I think we may flicker the icons in the url bar
//   as we bounce from URL to URL, without any script actually being executed.
// - Frames. This won't show icons for content_scripts running in frames. Should
//   it?
// - Possibly other weirdness where the state here doesn't match the state in
//   the renderer for whatever reason.
class ScriptBadgeController : public LocationBarController,
                              public ScriptExecutor,
                              public content::WebContentsObserver {
 public:
  explicit ScriptBadgeController(TabContentsWrapper* tab_contents);
  virtual ~ScriptBadgeController();

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

  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  // Gets the script badge for |extension|.
  ExtensionAction* GetScriptBadge(const Extension* extension);

  // Gets the ExtensionService for |tab_contents_|.
  ExtensionService* GetExtensionService();

  // Delegate ScriptExecutorImpl for running ExecuteScript.
  ScriptExecutorImpl script_executor_;

  // Our parent TabContentsWrapper.
  TabContentsWrapper* tab_contents_;

  // The extensions that have called ExecuteScript on the current frame.
  std::set<std::string> extensions_executing_scripts_;

  // Script badges that have been generated for extensions. This is both those
  // with actions already declared that are copied and normalised, and actions
  // that get generated for extensions that haven't declared anything.
  typedef std::map<std::string, linked_ptr<ExtensionAction> > ScriptBadgeMap;
  ScriptBadgeMap script_badges_;

  DISALLOW_COPY_AND_ASSIGN(ScriptBadgeController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCRIPT_BADGE_CONTROLLER_H_
