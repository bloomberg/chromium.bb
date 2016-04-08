// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MOCK_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_
#define CHROME_BROWSER_UI_TOOLBAR_MOCK_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_

#include "base/macros.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

class Browser;

// Mock class used to substitute ComponentToolbarActionsFactory in tests.
class MockComponentToolbarActionsFactory
    : public ComponentToolbarActionsFactory {
 public:
  static const char kActionIdForTesting[];

  explicit MockComponentToolbarActionsFactory(Browser* browser);
  ~MockComponentToolbarActionsFactory() override;

  // ComponentToolbarActionsFactory:
  std::set<std::string> GetInitialComponentIds(Profile* profile) override;
  std::unique_ptr<ToolbarActionViewController> GetComponentToolbarActionForId(
      const std::string& id,
      Browser* browser,
      ToolbarActionsBar* bar) override;

  void RegisterComponentMigrations(
      extensions::ComponentMigrationHelper* helper) const override;

  void HandleComponentMigrations(extensions::ComponentMigrationHelper* helper,
                                 Profile* profile) const override;

  void set_migrated_extension_id(const extensions::ExtensionId& extension_id) {
    migrated_extension_id_ = extension_id;
  }

  void set_migrated_feature_enabled(bool enabled) {
    migrated_feature_enabled_ = enabled;
  }

 private:
  extensions::ExtensionId migrated_extension_id_;
  bool migrated_feature_enabled_;

  DISALLOW_COPY_AND_ASSIGN(MockComponentToolbarActionsFactory);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MOCK_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_
