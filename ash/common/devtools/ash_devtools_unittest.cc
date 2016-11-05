// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/devtools/ash_devtools_dom_agent.h"

#include "ash/common/test/ash_test.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {
using namespace ui::devtools::protocol;
const int kDefaultChildNodeCount = -1;

class TestView : public views::View {
 public:
  TestView(const char* name) : views::View(), name_(name) {}

  const char* GetClassName() const override { return name_; }

 private:
  const char* name_;

  DISALLOW_COPY_AND_ASSIGN(TestView);
};

std::string GetAttributeValue(const std::string& attribute, DOM::Node* node) {
  EXPECT_TRUE(node->hasAttributes());
  Array<std::string>* attributes = node->getAttributes(nullptr);
  for (size_t i = 0; i < attributes->length() - 1; i++) {
    if (attributes->get(i) == attribute)
      return attributes->get(i + 1);
  }
  return nullptr;
}

bool Equals(views::Widget* widget, DOM::Node* node) {
  return "Widget" == node->getNodeName() &&
         widget->GetName() == GetAttributeValue("name", node) &&
         (widget->GetRootView() ? 1 : 0) ==
             node->getChildNodeCount(kDefaultChildNodeCount);
}

bool Equals(WmWindow* window, DOM::Node* node) {
  return "Window" == node->getNodeName() &&
         window->GetName() == GetAttributeValue("name", node) &&
         static_cast<int>(window->GetChildren().size()) ==
             node->getChildNodeCount(kDefaultChildNodeCount);
}

void Compare(views::View* view, DOM::Node* node) {
  EXPECT_EQ("View", node->getNodeName());
  EXPECT_EQ(view->GetClassName(), GetAttributeValue("name", node));
  EXPECT_EQ(view->child_count(),
            node->getChildNodeCount(kDefaultChildNodeCount));
}

void Compare(WmWindow* window, DOM::Node* node) {
  EXPECT_TRUE(Equals(window, node));
}

DOM::Node* FindInRoot(WmWindow* window, DOM::Node* root) {
  if (Equals(window, root))
    return root;

  Array<DOM::Node>* children = root->getChildren(nullptr);
  DOM::Node* window_node = nullptr;
  for (size_t i = 0; i < children->length(); i++) {
    window_node = FindInRoot(window, children->get(i));
    if (window_node)
      return window_node;
  }
  return window_node;
}

DOM::Node* FindInRoot(views::Widget* widget, DOM::Node* root) {
  if (Equals(widget, root))
    return root;

  Array<DOM::Node>* children = root->getChildren(nullptr);
  DOM::Node* widget_node = nullptr;
  for (size_t i = 0; i < children->length(); i++) {
    widget_node = FindInRoot(widget, children->get(i));
    if (widget_node)
      return widget_node;
  }
  return widget_node;
}

}  // namespace

class AshDevToolsTest : public AshTest {
 public:
  AshDevToolsTest() {}
  ~AshDevToolsTest() override {}

  void SetUp() override {
    AshTest::SetUp();
    dom_agent_ =
        base::MakeUnique<devtools::AshDevToolsDOMAgent>(WmShell::Get());
  }

  void TearDown() override {
    dom_agent_.reset();
    AshTest::TearDown();
  }

  devtools::AshDevToolsDOMAgent* dom_agent() { return dom_agent_.get(); }

 private:
  std::unique_ptr<devtools::AshDevToolsDOMAgent> dom_agent_;

  DISALLOW_COPY_AND_ASSIGN(AshDevToolsTest);
};

TEST_F(AshDevToolsTest, GetDocumentWithWindowWidgetView) {
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(gfx::Rect(1, 1, 1, 1)));
  WmWindow* parent_window = WmLookup::Get()->GetWindowForWidget(widget.get());
  parent_window->SetName("parent_window");
  std::unique_ptr<WindowOwner> child_owner(CreateChildWindow(parent_window));
  WmWindow* child_window = child_owner->window();
  child_window->SetName("child_window");
  widget->Show();
  views::View* child_view = new TestView("child_view");
  widget->GetRootView()->AddChildView(child_view);

  std::unique_ptr<ui::devtools::protocol::DOM::Node> root;

  dom_agent()->getDocument(&root);
  DOM::Node* parent_node = FindInRoot(parent_window, root.get());
  DOM::Node* widget_node = FindInRoot(widget.get(), root.get());

  ASSERT_TRUE(parent_node);
  ASSERT_TRUE(widget_node);
  Array<DOM::Node>* default_children = nullptr;
  ASSERT_TRUE(parent_node->getChildren(default_children));
  Compare(child_window, parent_node->getChildren(default_children)->get(0));
  Array<DOM::Node>* widget_children =
      widget_node->getChildren(default_children);
  ASSERT_TRUE(widget_children);
  Compare(widget->GetRootView(), widget_children->get(0));
  ASSERT_TRUE(widget_children->get(0)->getChildren(default_children));
  Compare(child_view,
          widget_children->get(0)->getChildren(default_children)->get(1));
  // TODO(mhashmi): Remove this call and mock FrontendChannel
  dom_agent()->disable();
}

}  // namespace ash
