// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_
#define CHROME_BROWSER_UI_TOOLBAR_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"

class Browser;
class ExtensionService;
class Profile;
class ToolbarActionViewController;

namespace extensions {
class ExtensionRegistry;
}

// The registry for all component toolbar actions. Component toolbar actions
// are actions that live in the toolbar (like extension actions), but are
// components of chrome, such as ChromeCast.
class ComponentToolbarActionsFactory {
 public:
  // Extension and component action IDs.
  static const char kCastBetaExtensionId[];
  static const char kCastExtensionId[];
  static const char kMediaRouterActionId[];

  ComponentToolbarActionsFactory();
  virtual ~ComponentToolbarActionsFactory();

  static ComponentToolbarActionsFactory* GetInstance();

  // Returns a vector of IDs of the component actions.
  virtual std::set<std::string> GetInitialComponentIds(Profile* profile);

  // Returns a collection of controllers for component actions. Declared
  // virtual for testing.
  virtual std::unique_ptr<ToolbarActionViewController>
  GetComponentToolbarActionForId(const std::string& id,
                                 Browser* browser,
                                 ToolbarActionsBar* bar);

  // Unloads extensions that were migrated to component actions and therefore
  // are no longer needed.
  void UnloadMigratedExtensions(ExtensionService* service,
                                extensions::ExtensionRegistry* registry);

  // Sets the factory to use for testing purposes.
  // Ownership remains with the caller.
  static void SetTestingFactory(ComponentToolbarActionsFactory* factory);

 private:
  // Unloads an extension if it is active.
  void UnloadExtension(ExtensionService* service,
                       extensions::ExtensionRegistry* registry,
                       const std::string& extension_id);

  DISALLOW_COPY_AND_ASSIGN(ComponentToolbarActionsFactory);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_
