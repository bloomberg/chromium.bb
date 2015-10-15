// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/mock_component_toolbar_actions_factory.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"

// static
const char MockComponentToolbarActionsFactory::kActionIdForTesting[] =
    "mock_action";

MockComponentToolbarActionsFactory::MockComponentToolbarActionsFactory(
    Browser* browser) {
  ComponentToolbarActionsFactory::SetTestingFactory(this);
}

MockComponentToolbarActionsFactory::~MockComponentToolbarActionsFactory() {
  ComponentToolbarActionsFactory::SetTestingFactory(nullptr);
}

std::set<std::string> MockComponentToolbarActionsFactory::GetComponentIds(
    Profile* profile) {
  std::set<std::string> ids;
  ids.insert(kActionIdForTesting);
  return ids;
}

scoped_ptr<ToolbarActionViewController>
MockComponentToolbarActionsFactory::GetComponentToolbarActionForId(
    const std::string& id,
    Browser* browser) {
  DCHECK_EQ(kActionIdForTesting, id);
  return scoped_ptr<ToolbarActionViewController>(
      new TestToolbarActionViewController(
          MockComponentToolbarActionsFactory::kActionIdForTesting));
}
