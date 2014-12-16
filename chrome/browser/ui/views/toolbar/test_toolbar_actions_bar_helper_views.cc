// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_helper.h"

#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "ui/views/view.h"

namespace {

// The views-specific implementation of the TestToolbarActionsBarHelper, which
// creates and owns a BrowserActionsContainer.
class TestToolbarActionsBarHelperViews : public TestToolbarActionsBarHelper {
 public:
  TestToolbarActionsBarHelperViews(Browser* browser,
                                   TestToolbarActionsBarHelperViews* main_bar);
  ~TestToolbarActionsBarHelperViews() override;

 private:
  // TestToolbarActionsBarHelper:
  ToolbarActionsBar* GetToolbarActionsBar() override;

  // The parent of the BrowserActionsContainer, which directly owns the
  // container as part of the views hierarchy.
  views::View container_parent_;

  // The created BrowserActionsContainer. Owned by |container_parent_|.
  BrowserActionsContainer* browser_actions_container_;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarActionsBarHelperViews);
};

TestToolbarActionsBarHelperViews::TestToolbarActionsBarHelperViews(
    Browser* browser,
    TestToolbarActionsBarHelperViews* main_bar)
    : browser_actions_container_(
          new BrowserActionsContainer(
              browser,
              main_bar ? main_bar->browser_actions_container_ : nullptr)) {
  // The BrowserActionsContainer expects to have a parent (and be added to the
  // view hierarchy), so wrap it in a shell view.
  container_parent_.set_owned_by_client();
  container_parent_.AddChildView(browser_actions_container_);
}

TestToolbarActionsBarHelperViews::~TestToolbarActionsBarHelperViews() {}

ToolbarActionsBar* TestToolbarActionsBarHelperViews::GetToolbarActionsBar() {
  return browser_actions_container_->toolbar_actions_bar();
}

}  // namespace

// static
scoped_ptr<TestToolbarActionsBarHelper>
TestToolbarActionsBarHelper::Create(Browser* browser,
                                    TestToolbarActionsBarHelper* main_bar) {
  return make_scoped_ptr(new TestToolbarActionsBarHelperViews(
      browser,
      static_cast<TestToolbarActionsBarHelperViews*>(main_bar)));
}
