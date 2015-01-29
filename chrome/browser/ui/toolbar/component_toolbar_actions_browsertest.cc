// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "extensions/common/feature_switch.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace {

const char kMockId[] = "mock_action";

class MockComponentAction : public ToolbarActionViewController {
 public:
  MockComponentAction() : click_count_(0u), id_(kMockId) {}
  ~MockComponentAction() override {}

  // ToolbarActionButtonController:
  const std::string& GetId() const override { return id_; }
  void SetDelegate(ToolbarActionViewDelegate* delegate) override {}
  gfx::Image GetIcon(content::WebContents* web_contents) override {
    return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_BROWSER_ACTION);
  }
  gfx::ImageSkia GetIconWithBadge() override {
    return *GetIcon(nullptr).ToImageSkia();
  }
  base::string16 GetActionName() const override {
    return base::ASCIIToUTF16("Component Action");
  }
  base::string16 GetAccessibleName(content::WebContents* web_contents)
      const override {
    return GetActionName();
  }
  base::string16 GetTooltip(content::WebContents* web_contents)
      const override {
    return GetActionName();
  }
  bool IsEnabled(content::WebContents* web_contents) const override {
    return true;
  }
  bool WantsToRun(content::WebContents* web_contents) const override {
    return false;
  }
  bool HasPopup(content::WebContents* web_contents) const override {
    return true;
  }
  void HidePopup() override {}
  gfx::NativeView GetPopupNativeView() override { return nullptr; }
  ui::MenuModel* GetContextMenu() override { return nullptr; }
  bool CanDrag() const override { return false; }
  bool IsMenuRunning() const override { return false; }
  bool ExecuteAction(bool by_user) override {
    ++click_count_;
    return false;
  }
  void UpdateState() override {}

  size_t click_count() const { return click_count_; }

 private:
  size_t click_count_;
  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(MockComponentAction);
};

class MockComponentToolbarActionsFactory
    : public ComponentToolbarActionsFactory {
 public:
  MockComponentToolbarActionsFactory();
  virtual ~MockComponentToolbarActionsFactory();

  // ComponentToolbarActionsFactory:
  ScopedVector<ToolbarActionViewController> GetComponentToolbarActions()
      override;

  const std::vector<MockComponentAction*>& weak_actions() const {
    return weak_actions_;
  }

 private:
  // A (weak) set of all created actions.
  std::vector<MockComponentAction*> weak_actions_;

  DISALLOW_COPY_AND_ASSIGN(MockComponentToolbarActionsFactory);
};

MockComponentToolbarActionsFactory::MockComponentToolbarActionsFactory() {
  ComponentToolbarActionsFactory::SetTestingFactory(this);
}

MockComponentToolbarActionsFactory::~MockComponentToolbarActionsFactory() {
  ComponentToolbarActionsFactory::SetTestingFactory(nullptr);
}

ScopedVector<ToolbarActionViewController>
MockComponentToolbarActionsFactory::GetComponentToolbarActions() {
  ScopedVector<ToolbarActionViewController> component_actions;
  MockComponentAction* action = new MockComponentAction();
  component_actions.push_back(action);
  weak_actions_.push_back(action);
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
  const std::vector<MockComponentAction*> weak_actions =
      mock_factory()->weak_actions();
  ASSERT_EQ(1u, weak_actions.size());
  MockComponentAction* mock_component_action = weak_actions[0];
  ASSERT_TRUE(mock_component_action);

  // Test that clicking on the component action works.
  EXPECT_EQ(0u, mock_component_action->click_count());
  browser_actions_bar.Press(0);
  EXPECT_EQ(1u, mock_component_action->click_count());
}
