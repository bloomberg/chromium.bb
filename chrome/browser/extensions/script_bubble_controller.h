// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCRIPT_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_SCRIPT_BUBBLE_CONTROLLER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "content/public/browser/web_contents_observer.h"

class ExtensionService;

namespace extensions {

class ExtensionSystem;

// Controls the script bubble in the omnibox, which displays information about
// extensions which are interacting with the current tab.
class ScriptBubbleController
    : public TabHelper::ScriptExecutionObserver,
      public content::WebContentsObserver {
 public:
  ScriptBubbleController(content::WebContents* web_contents,
                         TabHelper* tab_helper);
  virtual ~ScriptBubbleController();

  // Returns a set of extension ids for extensions running content scripts.
  const std::set<std::string>& extensions_running_scripts() {
    return extensions_running_scripts_;
  }

  // TabHelper::ScriptExecutionObserver implementation
  virtual void OnScriptsExecuted(
      const content::WebContents* web_contents,
      const ExecutingScriptsMap& extension_ids,
      int32 page_id,
      const GURL& on_url) OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  void OnExtensionUnloaded(const std::string& extension_id);

 private:
  // Helper to get the profile of the web contents we're associated with.
  Profile* profile() const;

  // Helper to get the extension service for the profile of the web contents
  // we're associated with.
  ExtensionService* GetExtensionService() const;

  // Helper to update the properties of the script bubble to reflect current
  // internal state.
  void UpdateScriptBubble();

  // The accumulated set of extension IDs that are operating on this tab.
  std::set<std::string> extensions_running_scripts_;

  DISALLOW_COPY_AND_ASSIGN(ScriptBubbleController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCRIPT_BUBBLE_CONTROLLER_H_
