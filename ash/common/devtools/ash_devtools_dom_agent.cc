// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/devtools/ash_devtools_dom_agent.h"

#include "ash/common/wm_lookup.h"
#include "ash/common/wm_window.h"
#include "components/ui_devtools/devtools_server.h"

namespace ash {
namespace devtools {

namespace {
using namespace ui::devtools::protocol;
// TODO(mhashmi): Make ids reusable
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
    RemoveWindowTree(params.target, true);
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
    AddWindowTree(params.target);
}

void AshDevToolsDOMAgent::OnWindowStackingChanged(WmWindow* window) {
  RemoveWindowTree(window, false);
  AddWindowTree(window);
}

WmWindow* AshDevToolsDOMAgent::GetWindowFromNodeId(int nodeId) {
  return node_id_to_window_map_.count(nodeId) ? node_id_to_window_map_[nodeId]
                                              : nullptr;
}

views::Widget* AshDevToolsDOMAgent::GetWidgetFromNodeId(int nodeId) {
  return node_id_to_widget_map_.count(nodeId) ? node_id_to_widget_map_[nodeId]
                                              : nullptr;
}

views::View* AshDevToolsDOMAgent::GetViewFromNodeId(int nodeId) {
  return node_id_to_view_map_.count(nodeId) ? node_id_to_view_map_[nodeId]
                                            : nullptr;
}

int AshDevToolsDOMAgent::GetNodeIdFromWindow(WmWindow* window) {
  DCHECK(window_to_node_id_map_.count(window));
  return window_to_node_id_map_[window];
}

int AshDevToolsDOMAgent::GetNodeIdFromWidget(views::Widget* widget) {
  DCHECK(widget_to_node_id_map_.count(widget));
  return widget_to_node_id_map_[widget];
}

int AshDevToolsDOMAgent::GetNodeIdFromView(views::View* view) {
  DCHECK(view_to_node_id_map_.count(view));
  return view_to_node_id_map_[view];
}

std::unique_ptr<ui::devtools::protocol::DOM::Node>
AshDevToolsDOMAgent::BuildInitialTree() {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  for (ash::WmWindow* window : shell_->GetAllRootWindows())
    children->addItem(BuildTreeForWindow(window));
  return BuildNode("root", nullptr, std::move(children));
}

std::unique_ptr<DOM::Node> AshDevToolsDOMAgent::BuildTreeForWindow(
    ash::WmWindow* window) {
  DCHECK(!window_to_node_id_map_.count(window));
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  views::Widget* widget = window->GetInternalWidget();
  if (widget)
    children->addItem(BuildTreeForRootWidget(widget));
  for (ash::WmWindow* child : window->GetChildren())
    children->addItem(BuildTreeForWindow(child));

  std::unique_ptr<ui::devtools::protocol::DOM::Node> node =
      BuildNode("Window", GetAttributes(window), std::move(children));
  if (!window->HasObserver(this))
    window->AddObserver(this);
  window_to_node_id_map_[window] = node->getNodeId();
  node_id_to_window_map_[node->getNodeId()] = window;
  return node;
}

std::unique_ptr<DOM::Node> AshDevToolsDOMAgent::BuildTreeForRootWidget(
    views::Widget* widget) {
  DCHECK(!widget_to_node_id_map_.count(widget));
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  children->addItem(BuildTreeForView(widget->GetRootView()));
  std::unique_ptr<ui::devtools::protocol::DOM::Node> node =
      BuildNode("Widget", GetAttributes(widget), std::move(children));
  // TODO(mhashmi): Add WidgetRemovalsObserver here
  widget_to_node_id_map_[widget] = node->getNodeId();
  node_id_to_widget_map_[node->getNodeId()] = widget;
  return node;
}

std::unique_ptr<DOM::Node> AshDevToolsDOMAgent::BuildTreeForView(
    views::View* view) {
  DCHECK(!view_to_node_id_map_.count(view));
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  for (int i = 0, count = view->child_count(); i < count; i++)
    children->addItem(BuildTreeForView(view->child_at(i)));
  std::unique_ptr<ui::devtools::protocol::DOM::Node> node =
      BuildNode("View", GetAttributes(view), std::move(children));
  // TODO(mhashmi): Add ViewObserver here
  view_to_node_id_map_[view] = node->getNodeId();
  node_id_to_view_map_[node->getNodeId()] = view;
  return node;
}

void AshDevToolsDOMAgent::AddWindowTree(WmWindow* window) {
  DCHECK(window_to_node_id_map_.count(window->GetParent()));
  WmWindow* prev_sibling = FindPreviousSibling(window);
  frontend()->childNodeInserted(
      window_to_node_id_map_[window->GetParent()],
      prev_sibling ? window_to_node_id_map_[prev_sibling] : 0,
      BuildTreeForWindow(window));
}

void AshDevToolsDOMAgent::RemoveWindowTree(WmWindow* window,
                                           bool remove_observer) {
  DCHECK(window);
  if (window->GetInternalWidget())
    RemoveWidgetTree(window->GetInternalWidget(), remove_observer);

  for (ash::WmWindow* child : window->GetChildren())
    RemoveWindowTree(child, remove_observer);

  RemoveWindowNode(window, remove_observer);
}

void AshDevToolsDOMAgent::RemoveWindowNode(WmWindow* window,
                                           bool remove_observer) {
  WindowToNodeIdMap::iterator window_to_node_id_it =
      window_to_node_id_map_.find(window);
  DCHECK(window_to_node_id_it != window_to_node_id_map_.end());

  int node_id = window_to_node_id_it->second;
  int parent_id = GetNodeIdFromWindow(window->GetParent());

  NodeIdToWindowMap::iterator node_id_to_window_it =
      node_id_to_window_map_.find(node_id);
  DCHECK(node_id_to_window_it != node_id_to_window_map_.end());

  if (remove_observer)
    window->RemoveObserver(this);
  node_id_to_window_map_.erase(node_id_to_window_it);
  window_to_node_id_map_.erase(window_to_node_id_it);
  frontend()->childNodeRemoved(parent_id, node_id);
}

void AshDevToolsDOMAgent::RemoveWidgetTree(views::Widget* widget,
                                           bool remove_observer) {
  DCHECK(widget);
  if (widget->GetRootView())
    RemoveViewTree(widget->GetRootView(), nullptr, remove_observer);
  RemoveWidgetNode(widget, remove_observer);
}

void AshDevToolsDOMAgent::RemoveWidgetNode(views::Widget* widget,
                                           bool remove_observer) {
  WidgetToNodeIdMap::iterator widget_to_node_id_it =
      widget_to_node_id_map_.find(widget);
  DCHECK(widget_to_node_id_it != widget_to_node_id_map_.end());

  int node_id = widget_to_node_id_it->second;
  int parent_id =
      GetNodeIdFromWindow(WmLookup::Get()->GetWindowForWidget(widget));

  // TODO(mhashmi): Add WidgetRemovalsObserver and remove it here based on
  // |remove_observer|

  NodeIdToWidgetMap::iterator node_id_to_widget_it =
      node_id_to_widget_map_.find(node_id);
  DCHECK(node_id_to_widget_it != node_id_to_widget_map_.end());

  widget_to_node_id_map_.erase(widget_to_node_id_it);
  node_id_to_widget_map_.erase(node_id_to_widget_it);
  frontend()->childNodeRemoved(parent_id, node_id);
}

void AshDevToolsDOMAgent::RemoveViewTree(views::View* view,
                                         views::View* parent,
                                         bool remove_observer) {
  DCHECK(view);
  for (int i = 0, count = view->child_count(); i < count; i++)
    RemoveViewTree(view->child_at(i), view, remove_observer);
  RemoveViewNode(view, parent, remove_observer);
}

void AshDevToolsDOMAgent::RemoveViewNode(views::View* view,
                                         views::View* parent,
                                         bool remove_observer) {
  ViewToNodeIdMap::iterator view_to_node_id_it =
      view_to_node_id_map_.find(view);
  DCHECK(view_to_node_id_it != view_to_node_id_map_.end());

  int node_id = view_to_node_id_it->second;
  int parent_id = 0;
  if (parent)
    parent_id = GetNodeIdFromView(parent);
  else  // views::RootView
    parent_id = GetNodeIdFromWidget(view->GetWidget());

  // TODO(mhashmi): Add ViewObserver and remove it here based on
  // |remove_observer|

  NodeIdToViewMap::iterator node_id_to_view_it =
      node_id_to_view_map_.find(node_id);
  DCHECK(node_id_to_view_it != node_id_to_view_map_.end());

  view_to_node_id_map_.erase(view_to_node_id_it);
  node_id_to_view_map_.erase(node_id_to_view_it);
  frontend()->childNodeRemoved(parent_id, node_id);
}

void AshDevToolsDOMAgent::RemoveObserverFromAllWindows() {
  for (auto& pair : window_to_node_id_map_)
    pair.first->RemoveObserver(this);
}

void AshDevToolsDOMAgent::Reset() {
  RemoveObserverFromAllWindows();
  window_to_node_id_map_.clear();
  widget_to_node_id_map_.clear();
  view_to_node_id_map_.clear();
  node_id_to_window_map_.clear();
  node_id_to_widget_map_.clear();
  node_id_to_view_map_.clear();
  node_ids = 1;
}

}  // namespace devtools
}  // namespace ash
