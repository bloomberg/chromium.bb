// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCRIPT_BADGE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_SCRIPT_BADGE_CONTROLLER_H_

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/script_executor.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

class ExtensionAction;
class ExtensionService;
class GURL;

namespace base {
class ListValue;
}  // namespace base

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
// - Observes a ScriptExecutor so that successfully-executed scripts
//   can cause a script badge to appear.
//
// When extension IDs are recorded a NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED
// is sent, and those extensions will be returned from GetCurrentActions until
// the next page navigation.
class ScriptBadgeController
    : public LocationBarController,
      public ScriptExecutor::Observer,
      public content::WebContentsObserver,
      public content::NotificationObserver {
 public:
  explicit ScriptBadgeController(content::WebContents* web_contents,
                                 ScriptExecutor* script_executor);
  virtual ~ScriptBadgeController();

  // LocationBarController implementation.
  virtual std::vector<ExtensionAction*> GetCurrentActions() const OVERRIDE;
  virtual void GetAttentionFor(const std::string& extension_id) OVERRIDE;
  virtual Action OnClicked(const std::string& extension_id,
                           int mouse_button) OVERRIDE;
  virtual void NotifyChange() OVERRIDE;

  // ScriptExecutor::Observer implementation.
  virtual void OnExecuteScriptFinished(
      const std::string& extension_id,
      const std::string& error,
      int32 on_page_id,
      const GURL& on_url,
      const base::ListValue& script_result) OVERRIDE;

 private:
  // Gets the ExtensionService for |tab_contents_|.
  ExtensionService* GetExtensionService();

  // Gets the current page ID, or -1 if no navigation entry has been committed.
  int32 GetPageID();

  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // IPC::Message handlers.
  void OnContentScriptsExecuting(const std::set<std::string>& extension_ids,
                                 int32 page_id,
                                 const GURL& on_url);

  // Adds the extension's icon to the list of script badges.  Returns
  // the script badge ExtensionAction that was added, or NULL if
  // extension_id isn't valid.
  ExtensionAction* AddExtensionToCurrentActions(
      const std::string& extension_id);

  // Called when an extension is running script on the current tab,
  // and tries to insert an extension into the relevant collections.
  // Returns true if any change was made.
  bool MarkExtensionExecuting(const std::string& extension_id);

  // Tries to erase an extension from the relevant collections, and returns
  // whether any change was made.
  bool EraseExtension(const Extension* extension);

  // The current extension actions in the order they appeared.  These come from
  // calls to ExecuteScript or getAttention on the current frame.
  std::vector<ExtensionAction*> current_actions_;

  // The extensions that have actions in current_actions_.
  std::set<std::string> extensions_in_current_actions_;

  // Listen to extension unloaded notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ScriptBadgeController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCRIPT_BADGE_CONTROLLER_H_
