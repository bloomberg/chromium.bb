// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_LOCATION_BAR_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_LOCATION_BAR_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "extensions/browser/extension_registry_observer.h"

class ExtensionAction;

namespace content {
class WebContents;
class BrowserContext;
}

namespace extensions {

class ActiveScriptController;
class Extension;
class ExtensionActionManager;
class ExtensionRegistry;

// Provides the UI with the current page actions for extensions. The execution
// of these actions is handled in the ExtensionActionAPI.
class LocationBarController : public ExtensionRegistryObserver {
 public:
  explicit LocationBarController(content::WebContents* web_contents);
  virtual ~LocationBarController();

  // Returns the actions which should be displayed in the location bar.
  std::vector<ExtensionAction*> GetCurrentActions();

  ActiveScriptController* active_script_controller() {
    return active_script_controller_.get();
  }

 private:
  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const Extension* extnesion) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // The associated WebContents.
  content::WebContents* web_contents_;

  // The associated BrowserContext.
  content::BrowserContext* browser_context_;

  // The ExtensionActionManager to provide page actions.
  ExtensionActionManager* action_manager_;

  // The ActiveScriptController, which could also add actions for extensions if
  // they have a pending script.
  scoped_ptr<ActiveScriptController> active_script_controller_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_LOCATION_BAR_CONTROLLER_H_
