// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/mock_component_toolbar_actions_factory.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"

MockComponentToolbarActionsFactory::MockComponentToolbarActionsFactory(
    Browser* browser) {
  ComponentToolbarActionsFactory::SetTestingFactory(this);

  ScopedVector<ToolbarActionViewController> actions =
      GetComponentToolbarActions(browser);
  for (const ToolbarActionViewController* action : actions)
    action_ids_.push_back(action->GetId());
}

MockComponentToolbarActionsFactory::~MockComponentToolbarActionsFactory() {
  ComponentToolbarActionsFactory::SetTestingFactory(nullptr);
}

ScopedVector<ToolbarActionViewController>
MockComponentToolbarActionsFactory::GetComponentToolbarActions(
    Browser* browser) {
  ScopedVector<ToolbarActionViewController> component_actions;
  component_actions.push_back(new TestToolbarActionViewController(
      ComponentToolbarActionsFactory::kActionIdForTesting));
  return component_actions.Pass();
}
