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

namespace extensions {

// Controls the script bubble in the omnibox, which displays information about
// extensions which are interacting with the current tab.
class ScriptBubbleController
    : public TabHelper::ContentScriptObserver,
      public content::WebContentsObserver {
 public:
  // Helper that gets the popup URL to use when the given set of extension ids
  // have scripts running in the tab.
  static GURL GetPopupUrl(const Extension* script_bubble,
                          const std::set<std::string>& extension_ids);

  ScriptBubbleController(content::WebContents* web_contents,
                         TabHelper* tab_helper);
  virtual ~ScriptBubbleController();

  // TabHelper::ContentScriptObserver implementation
  virtual void OnContentScriptsExecuting(
      const content::WebContents* web_contents,
      const ExecutingScriptsMap& extension_ids,
      int32 page_id,
      const GURL& on_url) OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  // Helper to get the extension service for the profile of the web contents
  // we're associated with.
  ExtensionService* GetExtensionService() const;

  // Helper to update the properties of the script bubble to reflect current
  // internal state.
  void UpdateScriptBubble();

  // The accumulated set of extension IDs that are operating on this tab.
  std::set<std::string> executing_extension_ids_;

  DISALLOW_COPY_AND_ASSIGN(ScriptBubbleController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCRIPT_BUBBLE_CONTROLLER_H_
