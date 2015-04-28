// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "extensions/common/feature_switch.h"

namespace {

const char kMockId[] = "mock_action";

class MockComponentToolbarActionsFactory
    : public ComponentToolbarActionsFactory {
 public:
  MockComponentToolbarActionsFactory();
  virtual ~MockComponentToolbarActionsFactory();

  // ComponentToolbarActionsFactory:
  ScopedVector<ToolbarActionViewController> GetComponentToolbarActions()
      override;

  const std::vector<std::string> action_ids() const {
    return action_ids_;
  }

 private:
  // A set of all action ids of created actions.
  std::vector<std::string> action_ids_;

  DISALLOW_COPY_AND_ASSIGN(MockComponentToolbarActionsFactory);
};

MockComponentToolbarActionsFactory::MockComponentToolbarActionsFactory() {
  ComponentToolbarActionsFactory::SetTestingFactory(this);

  ScopedVector<ToolbarActionViewController> actions =
      GetComponentToolbarActions();
  for (auto it = actions.begin(); it != actions.end(); ++it) {
    action_ids_.push_back((*it)->GetId());
  }
}

MockComponentToolbarActionsFactory::~MockComponentToolbarActionsFactory() {
  ComponentToolbarActionsFactory::SetTestingFactory(nullptr);
}

ScopedVector<ToolbarActionViewController>
MockComponentToolbarActionsFactory::GetComponentToolbarActions() {
  ScopedVector<ToolbarActionViewController> component_actions;
  TestToolbarActionViewController* action =
      new TestToolbarActionViewController(kMockId);
  component_actions.push_back(action);
  return component_actions.Pass();
}

}  // namespace

class ComponentToolbarActionsBrowserTest : public InProcessBrowserTest {
 protected:
  ComponentToolbarActionsBrowserTest() {}
  ~ComponentToolbarActionsBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    enable_redesign_.reset(new extensions::FeatureSwitch::ScopedOverride(
        extensions::FeatureSwitch::extension_action_redesign(), true));
    mock_actions_factory_.reset(new MockComponentToolbarActionsFactory());
  }

  MockComponentToolbarActionsFactory* mock_factory() {
    return mock_actions_factory_.get();
  }

 private:
  scoped_ptr<extensions::FeatureSwitch::ScopedOverride> enable_redesign_;
  scoped_ptr<MockComponentToolbarActionsFactory> mock_actions_factory_;

  DISALLOW_COPY_AND_ASSIGN(ComponentToolbarActionsBrowserTest);
};

// Test that Component Toolbar Actions appear in the browser actions container
// and can receive click events properly.
IN_PROC_BROWSER_TEST_F(ComponentToolbarActionsBrowserTest,
                       ComponentToolbarActionsShowUpAndRespondToClicks) {
  BrowserActionTestUtil browser_actions_bar(browser());

  // There should be only one component action view.
  ASSERT_EQ(1, browser_actions_bar.NumberOfBrowserActions());

  // Even though the method says "ExtensionId", this actually refers to any id
  // for the action.
  EXPECT_EQ(kMockId, browser_actions_bar.GetExtensionId(0));

  // There should only have been one created component action.
  const std::vector<std::string> action_ids = mock_factory()->action_ids();
  ASSERT_EQ(1u, action_ids.size());

  const std::vector<ToolbarActionViewController*>& actions =
      browser_actions_bar.GetToolbarActionsBar()->GetActions();
  TestToolbarActionViewController* mock_component_action =
      static_cast<TestToolbarActionViewController* const>(actions[0]);
  ASSERT_TRUE(mock_component_action);

  // Test that clicking on the component action works.
  EXPECT_EQ(0, mock_component_action->execute_action_count());
  browser_actions_bar.Press(0);
  EXPECT_EQ(1, mock_component_action->execute_action_count());
}
