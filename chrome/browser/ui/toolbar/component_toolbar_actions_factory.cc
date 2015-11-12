// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "extensions/common/feature_switch.h"

#if defined(ENABLE_MEDIA_ROUTER)
#include "chrome/browser/ui/toolbar/media_router_action.h"
#endif

namespace {

ComponentToolbarActionsFactory* testing_factory_ = nullptr;

base::LazyInstance<ComponentToolbarActionsFactory> lazy_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
const char ComponentToolbarActionsFactory::kMediaRouterActionId[] =
    "media_router_action";

ComponentToolbarActionsFactory::ComponentToolbarActionsFactory() {}
ComponentToolbarActionsFactory::~ComponentToolbarActionsFactory() {}

// static
ComponentToolbarActionsFactory* ComponentToolbarActionsFactory::GetInstance() {
  return testing_factory_ ? testing_factory_ : &lazy_factory.Get();
}

std::set<std::string> ComponentToolbarActionsFactory::GetComponentIds(
    Profile* profile) {
  std::set<std::string> component_ids;

  // This is currently behind the extension-action-redesign flag, as it is
  // designed for the new toolbar.
  if (!extensions::FeatureSwitch::extension_action_redesign()->IsEnabled())
    return component_ids;

  if (!profile->IsOffTheRecord() && media_router::MediaRouterEnabled(profile))
    component_ids.insert(kMediaRouterActionId);

  return component_ids;
}

scoped_ptr<ToolbarActionViewController>
ComponentToolbarActionsFactory::GetComponentToolbarActionForId(
    const std::string& id,
    Browser* browser) {
  // This is currently behind the extension-action-redesign flag, as it is
  // designed for the new toolbar.
  DCHECK(extensions::FeatureSwitch::extension_action_redesign()->IsEnabled());
  DCHECK(GetComponentIds(browser->profile()).count(id));

  // Add component toolbar actions here.
  // This current design means that the ComponentToolbarActionsFactory is aware
  // of all actions. Since we should *not* have an excessive amount of these
  // (since each will have an action in the toolbar or overflow menu), this
  // should be okay. If this changes, we should rethink this design to have,
  // e.g., RegisterChromeAction().
#if defined(ENABLE_MEDIA_ROUTER)
  if (id == kMediaRouterActionId)
    return scoped_ptr<ToolbarActionViewController>(
        new MediaRouterAction(browser));
#endif  // defined(ENABLE_MEDIA_ROUTER)

  NOTREACHED();
  return scoped_ptr<ToolbarActionViewController>();
}

// static
void ComponentToolbarActionsFactory::SetTestingFactory(
    ComponentToolbarActionsFactory* factory) {
  testing_factory_ = factory;
}
