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
class Profile;
class ToolbarActionViewController;

namespace extensions {
class ComponentMigrationHelper;
}  // extensions

// The registry for all component toolbar actions. Component toolbar actions
// are actions that live in the toolbar (like extension actions), but are
// components of chrome, such as ChromeCast.
class ComponentToolbarActionsFactory {
 public:
  // Component action IDs.
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

  // Registers component actions that are migrating from extensions.
  virtual void RegisterComponentMigrations(
      extensions::ComponentMigrationHelper* helper) const;

  // Synchronizes component action visibility and extension install status.
  virtual void HandleComponentMigrations(
      extensions::ComponentMigrationHelper* helper,
      Profile* profile) const;

  // Sets the factory to use for testing purposes.
  // Ownership remains with the caller.
  static void SetTestingFactory(ComponentToolbarActionsFactory* factory);

 private:
  DISALLOW_COPY_AND_ASSIGN(ComponentToolbarActionsFactory);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_
