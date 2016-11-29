// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/devtools/ash_devtools_dom_agent.h"

#include "ash/common/test/ash_test.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/strings/stringprintf.h"
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

class FakeFrontendChannel : public FrontendChannel {
 public:
  FakeFrontendChannel() {}
  ~FakeFrontendChannel() override {}

  int CountProtocolNotificationMessageStartsWith(const std::string& message) {
    int count = 0;
    for (const std::string& s : protocol_notification_messages_) {
      if (base::StartsWith(s, message, base::CompareCase::SENSITIVE))
        count++;
    }
    return count;
  }

  int CountProtocolNotificationMessage(const std::string& message) {
    return std::count(protocol_notification_messages_.begin(),
                      protocol_notification_messages_.end(), message);
  }

  // FrontendChannel
  void sendProtocolResponse(int callId,
                            std::unique_ptr<Serializable> message) override {}
  void flushProtocolNotifications() override {}
  void sendProtocolNotification(
      std::unique_ptr<Serializable> message) override {
    protocol_notification_messages_.push_back(message->serialize());
  }

 private:
  std::vector<std::string> protocol_notification_messages_;

  DISALLOW_COPY_AND_ASSIGN(FakeFrontendChannel);
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

bool Equals(WmWindow* window, DOM::Node* node) {
  int children_count = static_cast<int>(window->GetChildren().size());
  if (window->GetInternalWidget())
    children_count++;
  return "Window" == node->getNodeName() &&
         window->GetName() == GetAttributeValue("name", node) &&
         children_count == node->getChildNodeCount(kDefaultChildNodeCount);
}

void Compare(views::Widget* widget, DOM::Node* node) {
  EXPECT_EQ("Widget", node->getNodeName());
  EXPECT_EQ(widget->GetName(), GetAttributeValue("name", node));
  EXPECT_EQ(widget->GetRootView() ? 1 : 0,
            node->getChildNodeCount(kDefaultChildNodeCount));
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

}  // namespace

class AshDevToolsTest : public AshTest {
 public:
  AshDevToolsTest() {}
  ~AshDevToolsTest() override {}

  void SetUp() override {
    AshTest::SetUp();
    fake_frontend_channel_ = base::MakeUnique<FakeFrontendChannel>();
    uber_dispatcher_ =
        base::MakeUnique<UberDispatcher>(fake_frontend_channel_.get());
    dom_agent_ =
        base::MakeUnique<devtools::AshDevToolsDOMAgent>(WmShell::Get());
    dom_agent_->Init(uber_dispatcher_.get());
  }

  void TearDown() override {
    dom_agent_.reset();
    uber_dispatcher_.reset();
    fake_frontend_channel_.reset();
    AshTest::TearDown();
  }

  void ExpectChildNodeInserted(int parent_id, int prev_sibling_id) {
    EXPECT_EQ(1, frontend_channel()->CountProtocolNotificationMessageStartsWith(
                     base::StringPrintf("{\"method\":\"DOM.childNodeInserted\","
                                        "\"params\":{\"parentNodeId\":%d,"
                                        "\"previousNodeId\":%d",
                                        parent_id, prev_sibling_id)));
  }

  void ExpectChildNodeRemoved(int parent_id, int node_id) {
    EXPECT_EQ(1, frontend_channel()->CountProtocolNotificationMessage(
                     base::StringPrintf(
                         "{\"method\":\"DOM.childNodeRemoved\",\"params\":{"
                         "\"parentNodeId\":%d,\"nodeId\":%d}}",
                         parent_id, node_id)));
  }

  FakeFrontendChannel* frontend_channel() {
    return fake_frontend_channel_.get();
  }

  devtools::AshDevToolsDOMAgent* dom_agent() { return dom_agent_.get(); }

 private:
  std::unique_ptr<UberDispatcher> uber_dispatcher_;
  std::unique_ptr<FakeFrontendChannel> fake_frontend_channel_;
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
  ASSERT_TRUE(parent_node);
  Array<DOM::Node>* parent_children = parent_node->getChildren(nullptr);
  ASSERT_TRUE(parent_children);
  DOM::Node* widget_node = parent_children->get(0);
  Compare(widget.get(), widget_node);
  Compare(child_window, parent_children->get(1));
  Array<DOM::Node>* widget_children = widget_node->getChildren(nullptr);
  ASSERT_TRUE(widget_children);
  Compare(widget->GetRootView(), widget_children->get(0));
  ASSERT_TRUE(widget_children->get(0)->getChildren(nullptr));
  Compare(child_view, widget_children->get(0)->getChildren(nullptr)->get(1));
}

TEST_F(AshDevToolsTest, WindowAddedChildNodeInserted) {
  // Initialize DOMAgent
  std::unique_ptr<ui::devtools::protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  WmWindow* parent_window = WmShell::Get()->GetPrimaryRootWindow();
  DOM::Node* parent_node = root->getChildren(nullptr)->get(0);
  Array<DOM::Node>* parent_node_children = parent_node->getChildren(nullptr);
  DOM::Node* sibling_node =
      parent_node_children->get(parent_node_children->length() - 1);

  std::unique_ptr<WindowOwner> child_owner(CreateChildWindow(parent_window));
  ExpectChildNodeInserted(parent_node->getNodeId(), sibling_node->getNodeId());
}

TEST_F(AshDevToolsTest, WindowDestroyedChildNodeRemoved) {
  // Initialize DOMAgent
  std::unique_ptr<ui::devtools::protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  WmWindow* parent_window =
      WmShell::Get()->GetPrimaryRootWindow()->GetChildren()[0];
  WmWindow* child_window = parent_window->GetChildren()[0];
  DOM::Node* root_node = root->getChildren(nullptr)->get(0);
  DOM::Node* parent_node = root_node->getChildren(nullptr)->get(0);
  DOM::Node* child_node = parent_node->getChildren(nullptr)->get(0);

  Compare(parent_window, parent_node);
  Compare(child_window, child_node);
  child_window->Destroy();
  ExpectChildNodeRemoved(parent_node->getNodeId(), child_node->getNodeId());
}

TEST_F(AshDevToolsTest, WindowReorganizedChildNodeRearranged) {
  // Initialize DOMAgent
  std::unique_ptr<ui::devtools::protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  WmWindow* root_window = WmShell::Get()->GetPrimaryRootWindow();
  WmWindow* target_window = root_window->GetChildren()[1];
  WmWindow* child_window = root_window->GetChildren()[0]->GetChildren()[0];

  DOM::Node* root_node = root->getChildren(nullptr)->get(0);
  DOM::Node* parent_node = root_node->getChildren(nullptr)->get(0);
  DOM::Node* target_node = root_node->getChildren(nullptr)->get(1);
  Array<DOM::Node>* target_node_children = target_node->getChildren(nullptr);
  DOM::Node* sibling_node =
      target_node_children->get(target_node_children->length() - 1);
  DOM::Node* child_node = parent_node->getChildren(nullptr)->get(0);

  Compare(target_window, target_node);
  Compare(child_window, child_node);
  target_window->AddChild(child_window);
  ExpectChildNodeRemoved(parent_node->getNodeId(), child_node->getNodeId());
  ExpectChildNodeInserted(target_node->getNodeId(), sibling_node->getNodeId());
}

#if defined(OS_WIN)
#define MAYBE_WindowReorganizedChildNodeRemovedAndInserted \
  DISABLED_WindowReorganizedChildNodeRemovedAndInserted
#else
#define MAYBE_WindowReorganizedChildNodeRemovedAndInserted \
  WindowReorganizedChildNodeRemovedAndInserted
#endif  // defined(OS_WIN)
TEST_F(AshDevToolsTest, MAYBE_WindowReorganizedChildNodeRemovedAndInserted) {
  WmWindow* root_window = WmShell::Get()->GetPrimaryRootWindow();
  WmWindow* target_window = root_window->GetChildren()[1];
  WmWindow* parent_window = root_window->GetChildren()[0];
  std::unique_ptr<WindowOwner> child_owner(CreateChildWindow(parent_window));
  WmWindow* child_window = child_owner->window();

  // Initialize DOMAgent
  std::unique_ptr<ui::devtools::protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);
  DOM::Node* root_node = root->getChildren(nullptr)->get(0);

  DOM::Node* parent_node = root_node->getChildren(nullptr)->get(0);
  DOM::Node* target_node = root_node->getChildren(nullptr)->get(1);
  Array<DOM::Node>* target_node_children = target_node->getChildren(nullptr);
  DOM::Node* sibling_node =
      target_node_children->get(target_node_children->length() - 1);
  Array<DOM::Node>* parent_node_children = parent_node->getChildren(nullptr);
  DOM::Node* child_node =
      parent_node_children->get(parent_node_children->length() - 1);

  Compare(target_window, target_node);
  Compare(child_window, child_node);
  parent_window->RemoveChild(child_window);
  target_window->AddChild(child_window);
  ExpectChildNodeRemoved(parent_node->getNodeId(), child_node->getNodeId());
  ExpectChildNodeInserted(target_node->getNodeId(), sibling_node->getNodeId());
}

TEST_F(AshDevToolsTest, WindowStackingChangedChildNodeRemovedAndInserted) {
  // Initialize DOMAgent
  std::unique_ptr<ui::devtools::protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  WmWindow* parent_window = WmShell::Get()->GetPrimaryRootWindow();
  WmWindow* child_window = parent_window->GetChildren()[0];
  WmWindow* target_window = parent_window->GetChildren()[1];

  DOM::Node* parent_node = root->getChildren(nullptr)->get(0);
  Array<DOM::Node>* parent_node_children = parent_node->getChildren(nullptr);
  DOM::Node* child_node = parent_node_children->get(0);
  DOM::Node* sibling_node = parent_node_children->get(1);
  int parent_id = parent_node->getNodeId();

  Compare(parent_window, parent_node);
  Compare(child_window, child_node);
  parent_window->StackChildAbove(child_window, target_window);
  ExpectChildNodeRemoved(parent_id, child_node->getNodeId());
  ExpectChildNodeInserted(parent_id, sibling_node->getNodeId());
}

}  // namespace ash
