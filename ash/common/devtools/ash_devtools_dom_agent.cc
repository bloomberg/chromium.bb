// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/devtools/ash_devtools_dom_agent.h"

#include "ash/common/wm_window.h"
#include "components/ui_devtools/devtools_server.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace devtools {

namespace {
using namespace ui::devtools::protocol;

std::unique_ptr<DOM::Node> BuildNode(
    const std::string& name,
    std::unique_ptr<Array<std::string>> attributes,
    std::unique_ptr<Array<DOM::Node>> children) {
  static DOM::NodeId node_ids = 0;
  constexpr int kDomElementNodeType = 1;
  return DOM::Node::create()
      .setNodeId(node_ids++)
      .setNodeName(name)
      .setNodeType(kDomElementNodeType)
      .setAttributes(std::move(attributes))
      .setChildNodeCount(children->length())
      .setChildren(std::move(children))
      .build();
}

std::unique_ptr<Array<std::string>> GetAttributes(const ash::WmWindow* window) {
  std::unique_ptr<Array<std::string>> attributes = Array<std::string>::create();
  attributes->addItem("name");
  attributes->addItem(window->GetName());
  attributes->addItem("active");
  attributes->addItem(window->IsActive() ? "true" : "false");
  return attributes;
}

std::unique_ptr<Array<std::string>> GetAttributes(const views::Widget* widget) {
  std::unique_ptr<Array<std::string>> attributes = Array<std::string>::create();
  attributes->addItem("name");
  attributes->addItem(widget->GetName());
  attributes->addItem("active");
  attributes->addItem(widget->IsActive() ? "true" : "false");
  return attributes;
}

std::unique_ptr<Array<std::string>> GetAttributes(const views::View* view) {
  std::unique_ptr<Array<std::string>> attributes = Array<std::string>::create();
  attributes->addItem("name");
  attributes->addItem(view->GetClassName());
  return attributes;
}

std::unique_ptr<DOM::Node> BuildTreeForView(views::View* view) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  for (int i = 0, count = view->child_count(); i < count; i++) {
    children->addItem(BuildTreeForView(view->child_at(i)));
  }
  return BuildNode("View", GetAttributes(view), std::move(children));
}

std::unique_ptr<DOM::Node> BuildTreeForRootWidget(views::Widget* widget) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  children->addItem(BuildTreeForView(widget->GetRootView()));
  return BuildNode("Widget", GetAttributes(widget), std::move(children));
}

std::unique_ptr<DOM::Node> BuildTreeForWindow(ash::WmWindow* window) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  for (ash::WmWindow* child : window->GetChildren()) {
    children->addItem(BuildTreeForWindow(child));
    views::Widget* widget = child->GetInternalWidget();
    if (widget)
      children->addItem(BuildTreeForRootWidget(widget));
  }
  return BuildNode("Window", GetAttributes(window), std::move(children));
}

}  // namespace

AshDevToolsDOMAgent::AshDevToolsDOMAgent(ash::WmShell* shell) : shell_(shell) {
  DCHECK(shell_);
}

AshDevToolsDOMAgent::~AshDevToolsDOMAgent() {}

std::unique_ptr<ui::devtools::protocol::DOM::Node>
AshDevToolsDOMAgent::BuildInitialTree() {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  for (ash::WmWindow* window : shell_->GetAllRootWindows()) {
    children->addItem(BuildTreeForWindow(window));
  }
  return BuildNode("root", nullptr, std::move(children));
}

void AshDevToolsDOMAgent::getDocument(
    ui::devtools::protocol::ErrorString* error,
    std::unique_ptr<ui::devtools::protocol::DOM::Node>* out_root) {
  *out_root = BuildInitialTree();
}

}  // namespace devtools
}  // namespace ash
