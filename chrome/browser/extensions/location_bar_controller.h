// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_LOCATION_BAR_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_LOCATION_BAR_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class WebContents;
}

class ExtensionAction;

namespace extensions {

class ActiveScriptController;
class Extension;
class ExtensionRegistry;
class PageActionController;

// Interface for a class that controls the the extension icons that show up in
// the location bar. Depending on switches, these icons can have differing
// behavior.
class LocationBarController : public content::WebContentsObserver,
                              public ExtensionRegistryObserver {
 public:
  // The action that the UI should take after executing |OnClicked|.
  enum Action {
    ACTION_NONE,
    ACTION_SHOW_POPUP,
    ACTION_SHOW_CONTEXT_MENU,
  };

  class ActionProvider {
   public:
    // Returns the action for the given extension, or NULL if there isn't one.
    virtual ExtensionAction* GetActionForExtension(
        const Extension* extension) = 0;

    // Handles a click on an extension action.
    virtual LocationBarController::Action OnClicked(
        const Extension* extension) = 0;

    // A notification that the WebContents has navigated in the main frame (and
    // not in page), so any state relating to the current page should likely be
    // reset.
    virtual void OnNavigated() = 0;

    // A notification that the given |extension| has been unloaded, and any
    // actions associated with it should be removed.
    // The location bar controller will update itself after this if needed, so
    // Providers should not call NotifyChange().
    virtual void OnExtensionUnloaded(const Extension* extension) {}
  };

  explicit LocationBarController(content::WebContents* web_contents);
  virtual ~LocationBarController();

  // Returns the actions which should be displayed in the location bar.
  std::vector<ExtensionAction*> GetCurrentActions();

  // Notifies this that an ExtensionAction has been clicked, and returns the
  // action which should be taken in response (if any).
  Action OnClicked(const ExtensionAction* action);

  // Notifies the window that the actions have changed.
  static void NotifyChange(content::WebContents* web_contents);

  ActiveScriptController* active_script_controller() {
    return active_script_controller_.get();
  }

 private:
  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // The associated WebContents.
  content::WebContents* web_contents_;

  // The controllers for different sources of actions in the location bar.
  // Currently, this is only page actions and active script actions, so we
  // explicitly own and create both. If there are ever more, it will be worth
  // considering making this class own a list of LocationBarControllerProviders
  // instead.
  scoped_ptr<ActiveScriptController> active_script_controller_;
  scoped_ptr<PageActionController> page_action_controller_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_LOCATION_BAR_CONTROLLER_H_
