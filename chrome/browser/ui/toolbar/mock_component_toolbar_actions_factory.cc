// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/mock_component_toolbar_actions_factory.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "extensions/common/feature_switch.h"

// static
const char MockComponentToolbarActionsFactory::kActionIdForTesting[] =
    "mock_action";

MockComponentToolbarActionsFactory::MockComponentToolbarActionsFactory(
    Browser* browser)
    : migrated_feature_enabled_(false) {
  ComponentToolbarActionsFactory::SetTestingFactory(this);
}

MockComponentToolbarActionsFactory::~MockComponentToolbarActionsFactory() {
  ComponentToolbarActionsFactory::SetTestingFactory(nullptr);
}

std::set<std::string>
MockComponentToolbarActionsFactory::GetInitialComponentIds(Profile* profile) {
  std::set<std::string> ids;
  // kActionIdForTesting is installed by default if we are not testing
  // a migration scenario.
  if (extensions::FeatureSwitch::extension_action_redesign()->IsEnabled() &&
      migrated_extension_id_.empty()) {
    ids.insert(kActionIdForTesting);
  }
  return ids;
}

std::unique_ptr<ToolbarActionViewController>
MockComponentToolbarActionsFactory::GetComponentToolbarActionForId(
    const std::string& id,
    Browser* browser,
    ToolbarActionsBar* bar) {
  DCHECK_EQ(kActionIdForTesting, id);
  return std::unique_ptr<ToolbarActionViewController>(
      new TestToolbarActionViewController(
          MockComponentToolbarActionsFactory::kActionIdForTesting));
}

void MockComponentToolbarActionsFactory::RegisterComponentMigrations(
    extensions::ComponentMigrationHelper* helper) const {
  if (!migrated_extension_id_.empty()) {
    helper->Register(kActionIdForTesting, migrated_extension_id_);
  }
}

void MockComponentToolbarActionsFactory::HandleComponentMigrations(
    extensions::ComponentMigrationHelper* helper,
    Profile* profile) const {
  if (migrated_extension_id_.empty())
    return;

  if (migrated_feature_enabled_) {
    helper->OnFeatureEnabled(kActionIdForTesting);
  } else {
    helper->OnFeatureDisabled(kActionIdForTesting);
  }
}
