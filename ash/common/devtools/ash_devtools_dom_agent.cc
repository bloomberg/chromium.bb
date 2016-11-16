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

// Handles removing windows.
void AshDevToolsDOMAgent::OnWindowTreeChanging(WmWindow* window,
                                               const TreeChangeParams& params) {
  // Only trigger this when window == params.old_parent.
  // Only removals are handled here. Removing a node can occur as a result of
  // reorganizing a window or just destroying it. OnWindowTreeChanged
  // is only called if there is a new_parent. The only case this method isn't
  // called is when adding a node because old_parent is then null.
  // Finally, We only trigger this  0 or 1 times as an old_parent will
  // either exist and only call this callback once, or not at all.
  if (window == params.old_parent)
    RemoveWindowNode(params.target);
}

// Handles adding windows.
void AshDevToolsDOMAgent::OnWindowTreeChanged(WmWindow* window,
                                              const TreeChangeParams& params) {
  // Only trigger this when window == params.new_parent.
  // If there is an old_parent + new_parent, then this window's node was
  // removed in OnWindowTreeChanging and will now be added to the new_parent.
  // If there is only a new_parent, OnWindowTreeChanging is never called and
  // the window is only added here.
  if (window == params.new_parent)
    AddWindowNode(params.target);
}

void AshDevToolsDOMAgent::OnWindowStackingChanged(WmWindow* window) {
  RemoveWindowNode(window);
  AddWindowNode(window);
}

std::unique_ptr<DOM::Node> AshDevToolsDOMAgent::BuildTreeForWindow(
    ash::WmWindow* window) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  views::Widget* widget = window->GetInternalWidget();
  if (widget)
    children->addItem(BuildTreeForRootWidget(widget));
  for (ash::WmWindow* child : window->GetChildren()) {
    children->addItem(BuildTreeForWindow(child));
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
  DCHECK(window_to_node_id_map_.count(window->GetParent()));
  WmWindow* prev_sibling = FindPreviousSibling(window);
  frontend()->childNodeInserted(
      window_to_node_id_map_[window->GetParent()],
      prev_sibling ? window_to_node_id_map_[prev_sibling] : 0,
      BuildTreeForWindow(window));
}

void AshDevToolsDOMAgent::RemoveWindowNode(WmWindow* window) {
  WmWindow* parent = window->GetParent();
  DCHECK(parent);
  DCHECK(window_to_node_id_map_.count(parent));
  WindowToNodeIdMap::iterator it = window_to_node_id_map_.find(window);
  DCHECK(it != window_to_node_id_map_.end());

  int node_id = it->second;
  int parent_id = window_to_node_id_map_[parent];

  window->RemoveObserver(this);
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
