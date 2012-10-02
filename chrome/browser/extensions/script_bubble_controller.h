// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCRIPT_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_SCRIPT_BUBBLE_CONTROLLER_H_

#include "base/basictypes.h"
#include "chrome/browser/extensions/tab_helper.h"

namespace extensions {

// Controls the script bubble in the omnibox, which displays information about
// extensions which are interacting with the current tab.
class ScriptBubbleController : public TabHelper::ContentScriptObserver {
 public:
  // |tab_helper| must outlive the created ScriptBadgeController.
  explicit ScriptBubbleController(TabHelper* tab_helper);

  // TabHelper::ContentScriptObserver implementation
  virtual void OnContentScriptsExecuting(
      const content::WebContents* web_contents,
      const ExecutingScriptsMap& extension_ids,
      int32 page_id,
      const GURL& on_url) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScriptBubbleController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCRIPT_BUBBLE_CONTROLLER_H_
