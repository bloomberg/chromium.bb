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
DOM::NodeId node_ids = 1;

std::unique_ptr<DOM::Node> BuildNode(
    const std::string& name,
    std::unique_ptr<Array<std::string>> attributes,
    std::unique_ptr<Array<DOM::Node>> children) {
  constexpr int kDomElementNodeType = 1;
  std::unique_ptr<DOM::Node> node = DOM::Node::create()
                                        .setNodeId(node_ids++)
                                        .setNodeName(name)
                                        .setNodeType(kDomElementNodeType)
                                        .setAttributes(std::move(attributes))
                                        .build();
  node->setChildNodeCount(children->length());
  node->setChildren(std::move(children));
  return node;
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

WmWindow* FindPreviousSibling(WmWindow* window) {
  std::vector<WmWindow*> siblings = window->GetParent()->GetChildren();
  std::vector<WmWindow*>::iterator it =
      std::find(siblings.begin(), siblings.end(), window);
  DCHECK(it != siblings.end());
  // If this is the first child of its parent, the previous sibling is null
  return it == siblings.begin() ? nullptr : *std::prev(it);
}

}  // namespace

AshDevToolsDOMAgent::AshDevToolsDOMAgent(ash::WmShell* shell) : shell_(shell) {
  DCHECK(shell_);
}

AshDevToolsDOMAgent::~AshDevToolsDOMAgent() {
  RemoveObserverFromAllWindows();
}

ui::devtools::protocol::Response AshDevToolsDOMAgent::disable() {
  Reset();
  return ui::devtools::protocol::Response::OK();
}

ui::devtools::protocol::Response AshDevToolsDOMAgent::getDocument(
    std::unique_ptr<ui::devtools::protocol::DOM::Node>* out_root) {
  *out_root = BuildInitialTree();
  return ui::devtools::protocol::Response::OK();
}

// Need to remove node in OnWindowDestroying because the window parent reference
// is gone in OnWindowDestroyed
void AshDevToolsDOMAgent::OnWindowDestroying(WmWindow* window) {
  RemoveWindowNode(window, window->GetParent());
}

void AshDevToolsDOMAgent::OnWindowTreeChanged(WmWindow* window,
                                              const TreeChangeParams& params) {
  // Only trigger this when window == root window.
  // Removals are handled on OnWindowDestroying.
  if (window != window->GetRootWindow() || !params.new_parent)
    return;
  // If there is an old_parent + new_parent, then this window is being moved
  // which requires a remove followed by an add. If only new_parent
  // exists, then a new window is being created so only add is called.
  if (params.old_parent)
    RemoveWindowNode(params.target, params.old_parent);
  AddWindowNode(params.target);
}

void AshDevToolsDOMAgent::OnWindowStackingChanged(WmWindow* window) {
  RemoveWindowNode(window, window->GetParent());
  AddWindowNode(window);
}

std::unique_ptr<DOM::Node> AshDevToolsDOMAgent::BuildTreeForWindow(
    ash::WmWindow* window) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  for (ash::WmWindow* child : window->GetChildren()) {
    children->addItem(BuildTreeForWindow(child));
    views::Widget* widget = child->GetInternalWidget();
    if (widget)
      children->addItem(BuildTreeForRootWidget(widget));
  }
  std::unique_ptr<ui::devtools::protocol::DOM::Node> node =
      BuildNode("Window", GetAttributes(window), std::move(children));
  // Only add as observer if window is not in map
  if (!window_to_node_id_map_.count(window))
    window->AddObserver(this);
  window_to_node_id_map_[window] = node->getNodeId();
  return node;
}

std::unique_ptr<ui::devtools::protocol::DOM::Node>
AshDevToolsDOMAgent::BuildInitialTree() {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  for (ash::WmWindow* window : shell_->GetAllRootWindows()) {
    children->addItem(BuildTreeForWindow(window));
  }
  return BuildNode("root", nullptr, std::move(children));
}

void AshDevToolsDOMAgent::AddWindowNode(WmWindow* window) {
  WmWindow* prev_sibling = FindPreviousSibling(window);
  frontend()->childNodeInserted(
      window_to_node_id_map_[window->GetParent()],
      prev_sibling ? window_to_node_id_map_[prev_sibling] : 0,
      BuildTreeForWindow(window));
}

void AshDevToolsDOMAgent::RemoveWindowNode(WmWindow* window,
                                           WmWindow* old_parent) {
  window->RemoveObserver(this);
  WindowToNodeIdMap::iterator it = window_to_node_id_map_.find(window);
  DCHECK(it != window_to_node_id_map_.end());

  int node_id = it->second;
  int parent_id = old_parent ? window_to_node_id_map_[old_parent] : 0;

  window_to_node_id_map_.erase(it);
  frontend()->childNodeRemoved(parent_id, node_id);
}

void AshDevToolsDOMAgent::RemoveObserverFromAllWindows() {
  for (auto& pair : window_to_node_id_map_)
    pair.first->RemoveObserver(this);
}

void AshDevToolsDOMAgent::Reset() {
  RemoveObserverFromAllWindows();
  window_to_node_id_map_.clear();
  node_ids = 1;
}

}  // namespace devtools
}  // namespace ash
