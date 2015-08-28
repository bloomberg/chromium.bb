// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/feature_switch.h"

namespace {

ComponentToolbarActionsFactory* testing_factory_ = nullptr;

base::LazyInstance<ComponentToolbarActionsFactory> lazy_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
const char ComponentToolbarActionsFactory::kMediaRouterActionId[] =
    "media_router_action";
const char ComponentToolbarActionsFactory::kActionIdForTesting[] =
    "mock_action";

ComponentToolbarActionsFactory::ComponentToolbarActionsFactory() {}
ComponentToolbarActionsFactory::~ComponentToolbarActionsFactory() {}

// static
ComponentToolbarActionsFactory* ComponentToolbarActionsFactory::GetInstance() {
  return testing_factory_ ? testing_factory_ : &lazy_factory.Get();
}

// static
std::vector<std::string> ComponentToolbarActionsFactory::GetComponentIds() {
  std::vector<std::string> component_ids;

  // This is currently behind the extension-action-redesign flag, as it is
  // designed for the new toolbar.
  if (!extensions::FeatureSwitch::extension_action_redesign()->IsEnabled())
    return component_ids;

  if (testing_factory_) {
    component_ids.push_back(
        ComponentToolbarActionsFactory::kActionIdForTesting);
  } else if (switches::MediaRouterEnabled()) {
    component_ids.push_back(
        ComponentToolbarActionsFactory::kMediaRouterActionId);
  }

  return component_ids;
}

// static
bool ComponentToolbarActionsFactory::EnabledIncognito(
    const std::string& action_id) {
  return action_id != kMediaRouterActionId;
}

ScopedVector<ToolbarActionViewController>
ComponentToolbarActionsFactory::GetComponentToolbarActions(Browser* browser) {
  ScopedVector<ToolbarActionViewController> component_actions;

  // This is currently behind the extension-action-redesign flag, as it is
  // designed for the new toolbar.
  if (!extensions::FeatureSwitch::extension_action_redesign()->IsEnabled())
    return component_actions.Pass();

  // Add component toolbar actions here.
  // This current design means that the ComponentToolbarActionsFactory is aware
  // of all actions. Since we should *not* have an excessive amount of these
  // (since each will have an action in the toolbar or overflow menu), this
  // should be okay. If this changes, we should rethink this design to have,
  // e.g., RegisterChromeAction().

  if (switches::MediaRouterEnabled() && !browser->profile()->IsOffTheRecord())
    component_actions.push_back(new MediaRouterAction(browser));

  return component_actions.Pass();
}

// static
void ComponentToolbarActionsFactory::SetTestingFactory(
    ComponentToolbarActionsFactory* factory) {
  testing_factory_ = factory;
}
