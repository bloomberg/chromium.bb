// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/dom_agent.h"

#include <memory>

#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/views/root_element.h"
#include "components/ui_devtools/views/ui_element.h"
#include "components/ui_devtools/views/view_element.h"
#include "components/ui_devtools/views/widget_element.h"
#include "components/ui_devtools/views/window_element.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ui_devtools {
namespace {

using namespace ui_devtools::protocol;
// TODO(mhashmi): Make ids reusable

std::unique_ptr<DOM::Node> BuildNode(
    const std::string& name,
    std::unique_ptr<Array<std::string>> attributes,
    std::unique_ptr<Array<DOM::Node>> children,
    int node_ids) {
  constexpr int kDomElementNodeType = 1;
  std::unique_ptr<DOM::Node> node = DOM::Node::create()
                                        .setNodeId(node_ids)
                                        .setBackendNodeId(node_ids)
                                        .setNodeName(name)
                                        .setNodeType(kDomElementNodeType)
                                        .setAttributes(std::move(attributes))
                                        .build();
  node->setChildNodeCount(static_cast<int>(children->length()));
  node->setChildren(std::move(children));
  return node;
}

views::Widget* GetWidgetFromWindow(gfx::NativeWindow window) {
  return views::Widget::GetWidgetForNativeView(window);
}

std::unique_ptr<DOM::Node> BuildDomNodeFromUIElement(UIElement* root) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  for (auto* it : root->children())
    children->addItem(BuildDomNodeFromUIElement(it));

  return BuildNode(root->GetTypeName(), root->GetAttributes(),
                   std::move(children), root->node_id());
}

}  // namespace

DOMAgent::DOMAgent() : is_building_tree_(false) {
  aura::Env::GetInstance()->AddObserver(this);
}

DOMAgent::~DOMAgent() {
  aura::Env::GetInstance()->RemoveObserver(this);
  Reset();
}

Response DOMAgent::disable() {
  Reset();
  return Response::OK();
}

Response DOMAgent::getDocument(std::unique_ptr<DOM::Node>* out_root) {
  *out_root = BuildInitialTree();
  return Response::OK();
}

Response DOMAgent::hideHighlight() {
  return Response::OK();
}

Response DOMAgent::pushNodesByBackendIdsToFrontend(
    std::unique_ptr<protocol::Array<int>> backend_node_ids,
    std::unique_ptr<protocol::Array<int>>* result) {
  *result = protocol::Array<int>::create();
  for (size_t index = 0; index < backend_node_ids->length(); ++index)
    (*result)->addItem(backend_node_ids->get(index));
  return Response::OK();
}

void DOMAgent::OnUIElementAdded(UIElement* parent, UIElement* child) {
  // When parent is null, only need to update |node_id_to_ui_element_|.
  if (!parent) {
    node_id_to_ui_element_[child->node_id()] = child;
    return;
  }
  // If tree is being built, don't add child to dom tree again.
  if (is_building_tree_)
    return;
  DCHECK(node_id_to_ui_element_.count(parent->node_id()));

  const auto& children = parent->children();
  auto iter = std::find(children.begin(), children.end(), child);
  int prev_node_id =
      (iter == children.end() - 1) ? 0 : (*std::next(iter))->node_id();
  frontend()->childNodeInserted(parent->node_id(), prev_node_id,
                                BuildTreeForUIElement(child));
}

void DOMAgent::OnUIElementReordered(UIElement* parent, UIElement* child) {
  DCHECK(node_id_to_ui_element_.count(parent->node_id()));

  const auto& children = parent->children();
  auto iter = std::find(children.begin(), children.end(), child);
  int prev_node_id =
      (iter == children.begin()) ? 0 : (*std::prev(iter))->node_id();
  RemoveDomNode(child);
  frontend()->childNodeInserted(parent->node_id(), prev_node_id,
                                BuildDomNodeFromUIElement(child));
}

void DOMAgent::OnUIElementRemoved(UIElement* ui_element) {
  DCHECK(node_id_to_ui_element_.count(ui_element->node_id()));

  RemoveDomNode(ui_element);
  node_id_to_ui_element_.erase(ui_element->node_id());
}

void DOMAgent::OnUIElementBoundsChanged(UIElement* ui_element) {
  for (auto& observer : observers_)
    observer.OnElementBoundsChanged(ui_element);
}

void DOMAgent::AddObserver(DOMAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void DOMAgent::RemoveObserver(DOMAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}

UIElement* DOMAgent::GetElementFromNodeId(int node_id) {
  return node_id_to_ui_element_[node_id];
}

int DOMAgent::GetParentIdOfNodeId(int node_id) const {
  DCHECK(node_id_to_ui_element_.count(node_id));
  const UIElement* element = node_id_to_ui_element_.at(node_id);
  if (element->parent() && element->parent() != element_root_.get())
    return element->parent()->node_id();
  return 0;
}

void DOMAgent::OnHostInitialized(aura::WindowTreeHost* host) {
  root_windows_.push_back(host->window());
}

void DOMAgent::OnElementBoundsChanged(UIElement* ui_element) {
  for (auto& observer : observers_)
    observer.OnElementBoundsChanged(ui_element);
}

std::unique_ptr<DOM::Node> DOMAgent::BuildInitialTree() {
  is_building_tree_ = true;
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();

  element_root_ = std::make_unique<RootElement>(this);

  for (aura::Window* window : root_windows()) {
    UIElement* window_element =
        new WindowElement(window, this, element_root_.get());

    children->addItem(BuildTreeForUIElement(window_element));
    element_root_->AddChild(window_element);
  }
  std::unique_ptr<DOM::Node> root_node =
      BuildNode("root", nullptr, std::move(children), element_root_->node_id());
  is_building_tree_ = false;
  return root_node;
}

std::unique_ptr<DOM::Node> DOMAgent::BuildTreeForUIElement(
    UIElement* ui_element) {
  if (ui_element->type() == UIElementType::WINDOW) {
    return BuildTreeForWindow(
        ui_element,
        UIElement::GetBackingElement<aura::Window, WindowElement>(ui_element));
  } else if (ui_element->type() == UIElementType::WIDGET) {
    return BuildTreeForRootWidget(
        ui_element,
        UIElement::GetBackingElement<views::Widget, WidgetElement>(ui_element));
  } else if (ui_element->type() == UIElementType::VIEW) {
    return BuildTreeForView(
        ui_element,
        UIElement::GetBackingElement<views::View, ViewElement>(ui_element));
  }
  return nullptr;
}

std::unique_ptr<DOM::Node> DOMAgent::BuildTreeForWindow(
    UIElement* window_element_root,
    aura::Window* window) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  views::Widget* widget = GetWidgetFromWindow(window);
  if (widget) {
    UIElement* widget_element =
        new WidgetElement(widget, this, window_element_root);

    children->addItem(BuildTreeForRootWidget(widget_element, widget));
    window_element_root->AddChild(widget_element);
  }
  for (aura::Window* child : window->children()) {
    UIElement* window_element =
        new WindowElement(child, this, window_element_root);

    children->addItem(BuildTreeForWindow(window_element, child));
    window_element_root->AddChild(window_element);
  }
  std::unique_ptr<DOM::Node> node =
      BuildNode("Window", window_element_root->GetAttributes(),
                std::move(children), window_element_root->node_id());
  return node;
}

std::unique_ptr<DOM::Node> DOMAgent::BuildTreeForRootWidget(
    UIElement* widget_element,
    views::Widget* widget) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();

  UIElement* view_element =
      new ViewElement(widget->GetRootView(), this, widget_element);

  children->addItem(BuildTreeForView(view_element, widget->GetRootView()));
  widget_element->AddChild(view_element);

  std::unique_ptr<DOM::Node> node =
      BuildNode("Widget", widget_element->GetAttributes(), std::move(children),
                widget_element->node_id());
  return node;
}

std::unique_ptr<DOM::Node> DOMAgent::BuildTreeForView(UIElement* view_element,
                                                      views::View* view) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();

  for (auto* child : view->GetChildrenInZOrder()) {
    UIElement* view_element_child = new ViewElement(child, this, view_element);

    children->addItem(BuildTreeForView(view_element_child, child));
    view_element->AddChild(view_element_child);
  }
  std::unique_ptr<DOM::Node> node =
      BuildNode("View", view_element->GetAttributes(), std::move(children),
                view_element->node_id());
  return node;
}

void DOMAgent::RemoveDomNode(UIElement* ui_element) {
  for (auto* child_element : ui_element->children())
    RemoveDomNode(child_element);
  frontend()->childNodeRemoved(ui_element->parent()->node_id(),
                               ui_element->node_id());
}

void DOMAgent::Reset() {
  is_building_tree_ = false;
  element_root_.reset();
  node_id_to_ui_element_.clear();
  observers_.Clear();
}

}  // namespace ui_devtools
