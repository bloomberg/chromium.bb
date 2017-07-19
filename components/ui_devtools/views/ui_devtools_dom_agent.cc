// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/ui_devtools_dom_agent.h"

#include "base/memory/ptr_util.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/views/ui_element.h"
#include "components/ui_devtools/views/view_element.h"
#include "components/ui_devtools/views/widget_element.h"
#include "components/ui_devtools/views/window_element.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
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
                                        .setNodeName(name)
                                        .setNodeType(kDomElementNodeType)
                                        .setAttributes(std::move(attributes))
                                        .build();
  node->setChildNodeCount(static_cast<int>(children->length()));
  node->setChildren(std::move(children));
  return node;
}

// TODO(thanhph): Move this function to UIElement::GetAttributes().
std::unique_ptr<Array<std::string>> GetAttributes(UIElement* ui_element) {
  std::unique_ptr<Array<std::string>> attributes = Array<std::string>::create();
  attributes->addItem("name");
  switch (ui_element->type()) {
    case UIElementType::WINDOW: {
      aura::Window* window =
          UIElement::GetBackingElement<aura::Window, WindowElement>(ui_element);
      attributes->addItem(window->GetName());
      attributes->addItem("active");
      attributes->addItem(::wm::IsActiveWindow(window) ? "true" : "false");
      break;
    }
    case UIElementType::WIDGET: {
      views::Widget* widget =
          UIElement::GetBackingElement<views::Widget, WidgetElement>(
              ui_element);
      attributes->addItem(widget->GetName());
      attributes->addItem("active");
      attributes->addItem(widget->IsActive() ? "true" : "false");
      break;
    }
    case UIElementType::VIEW: {
      attributes->addItem(
          UIElement::GetBackingElement<views::View, ViewElement>(ui_element)
              ->GetClassName());
      break;
    }
    default:
      DCHECK(false);
  }
  return attributes;
}

int MaskColor(int value) {
  return value & 0xff;
}

SkColor RGBAToSkColor(DOM::RGBA* rgba) {
  if (!rgba)
    return SkColorSetARGB(0, 0, 0, 0);
  // Default alpha value is 0 (not visible) and need to convert alpha decimal
  // percentage value to hex
  return SkColorSetARGB(MaskColor(static_cast<int>(rgba->getA(0) * 255)),
                        MaskColor(rgba->getR()), MaskColor(rgba->getG()),
                        MaskColor(rgba->getB()));
}

views::Widget* GetWidgetFromWindow(aura::Window* window) {
  return views::Widget::GetWidgetForNativeView(window);
}

std::unique_ptr<DOM::Node> BuildDomNodeFromUIElement(UIElement* root) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();
  for (auto* it : root->children())
    children->addItem(BuildDomNodeFromUIElement(it));

  constexpr int kDomElementNodeType = 1;
  std::unique_ptr<DOM::Node> node = DOM::Node::create()
                                        .setNodeId(root->node_id())
                                        .setNodeName(root->GetTypeName())
                                        .setNodeType(kDomElementNodeType)
                                        .setAttributes(GetAttributes(root))
                                        .build();
  node->setChildNodeCount(static_cast<int>(children->length()));
  node->setChildren(std::move(children));
  return node;
}

}  // namespace

UIDevToolsDOMAgent::UIDevToolsDOMAgent() : is_building_tree_(false) {
  aura::Env::GetInstance()->AddObserver(this);
}

UIDevToolsDOMAgent::~UIDevToolsDOMAgent() {
  aura::Env::GetInstance()->RemoveObserver(this);
  Reset();
}

ui_devtools::protocol::Response UIDevToolsDOMAgent::disable() {
  Reset();
  return ui_devtools::protocol::Response::OK();
}

ui_devtools::protocol::Response UIDevToolsDOMAgent::getDocument(
    std::unique_ptr<ui_devtools::protocol::DOM::Node>* out_root) {
  *out_root = BuildInitialTree();
  return ui_devtools::protocol::Response::OK();
}

ui_devtools::protocol::Response UIDevToolsDOMAgent::highlightNode(
    std::unique_ptr<ui_devtools::protocol::DOM::HighlightConfig>
        highlight_config,
    ui_devtools::protocol::Maybe<int> node_id) {
  return HighlightNode(std::move(highlight_config), node_id.fromJust());
}

ui_devtools::protocol::Response UIDevToolsDOMAgent::hideHighlight() {
  if (layer_for_highlighting_ && layer_for_highlighting_->visible())
    layer_for_highlighting_->SetVisible(false);
  return ui_devtools::protocol::Response::OK();
}

void UIDevToolsDOMAgent::OnUIElementAdded(UIElement* parent, UIElement* child) {
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

void UIDevToolsDOMAgent::OnUIElementReordered(UIElement* parent,
                                              UIElement* child) {
  DCHECK(node_id_to_ui_element_.count(parent->node_id()));

  const auto& children = parent->children();
  auto iter = std::find(children.begin(), children.end(), child);
  int prev_node_id =
      (iter == children.begin()) ? 0 : (*std::prev(iter))->node_id();
  RemoveDomNode(child);
  frontend()->childNodeInserted(parent->node_id(), prev_node_id,
                                BuildDomNodeFromUIElement(child));
}

void UIDevToolsDOMAgent::OnUIElementRemoved(UIElement* ui_element) {
  DCHECK(node_id_to_ui_element_.count(ui_element->node_id()));

  RemoveDomNode(ui_element);
  node_id_to_ui_element_.erase(ui_element->node_id());
}

void UIDevToolsDOMAgent::OnUIElementBoundsChanged(UIElement* ui_element) {
  for (auto& observer : observers_)
    observer.OnNodeBoundsChanged(ui_element->node_id());
}

void UIDevToolsDOMAgent::AddObserver(UIDevToolsDOMAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void UIDevToolsDOMAgent::RemoveObserver(UIDevToolsDOMAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}

UIElement* UIDevToolsDOMAgent::GetElementFromNodeId(int node_id) {
  return node_id_to_ui_element_[node_id];
}

void UIDevToolsDOMAgent::OnHostInitialized(aura::WindowTreeHost* host) {
  root_windows_.push_back(host->window());
}

void UIDevToolsDOMAgent::OnNodeBoundsChanged(int node_id) {
  for (auto& observer : observers_)
    observer.OnNodeBoundsChanged(node_id);
}

std::unique_ptr<ui_devtools::protocol::DOM::Node>
UIDevToolsDOMAgent::BuildInitialTree() {
  is_building_tree_ = true;
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();

  // TODO(thanhph): Root of UIElement tree shoudn't be WindowElement
  // but maybe a new different element type.
  window_element_root_ =
      base::MakeUnique<WindowElement>(nullptr, this, nullptr);

  for (aura::Window* window : root_windows()) {
    UIElement* window_element =
        new WindowElement(window, this, window_element_root_.get());

    children->addItem(BuildTreeForUIElement(window_element));
    window_element_root_->AddChild(window_element);
  }
  std::unique_ptr<ui_devtools::protocol::DOM::Node> root_node = BuildNode(
      "root", nullptr, std::move(children), window_element_root_->node_id());
  is_building_tree_ = false;
  return root_node;
}

std::unique_ptr<ui_devtools::protocol::DOM::Node>
UIDevToolsDOMAgent::BuildTreeForUIElement(UIElement* ui_element) {
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

std::unique_ptr<DOM::Node> UIDevToolsDOMAgent::BuildTreeForWindow(
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
  std::unique_ptr<ui_devtools::protocol::DOM::Node> node =
      BuildNode("Window", GetAttributes(window_element_root),
                std::move(children), window_element_root->node_id());
  return node;
}

std::unique_ptr<DOM::Node> UIDevToolsDOMAgent::BuildTreeForRootWidget(
    UIElement* widget_element,
    views::Widget* widget) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();

  UIElement* view_element =
      new ViewElement(widget->GetRootView(), this, widget_element);

  children->addItem(BuildTreeForView(view_element, widget->GetRootView()));
  widget_element->AddChild(view_element);

  std::unique_ptr<ui_devtools::protocol::DOM::Node> node =
      BuildNode("Widget", GetAttributes(widget_element), std::move(children),
                widget_element->node_id());
  return node;
}

std::unique_ptr<DOM::Node> UIDevToolsDOMAgent::BuildTreeForView(
    UIElement* view_element,
    views::View* view) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();

  for (auto* child : view->GetChildrenInZOrder()) {
    UIElement* view_element_child = new ViewElement(child, this, view_element);

    children->addItem(BuildTreeForView(view_element_child, child));
    view_element->AddChild(view_element_child);
  }
  std::unique_ptr<ui_devtools::protocol::DOM::Node> node =
      BuildNode("View", GetAttributes(view_element), std::move(children),
                view_element->node_id());
  return node;
}

void UIDevToolsDOMAgent::RemoveDomNode(UIElement* ui_element) {
  for (auto* child_element : ui_element->children())
    RemoveDomNode(child_element);
  frontend()->childNodeRemoved(ui_element->parent()->node_id(),
                               ui_element->node_id());
}

void UIDevToolsDOMAgent::Reset() {
  is_building_tree_ = false;
  layer_for_highlighting_.reset();
  window_element_root_.reset();
  node_id_to_ui_element_.clear();
  observers_.Clear();
}

void UIDevToolsDOMAgent::UpdateHighlight(
    const std::pair<aura::Window*, gfx::Rect>& window_and_bounds,
    SkColor background) {
  layer_for_highlighting_->SetColor(background);

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          window_and_bounds.first);
  aura::Window* root = window_and_bounds.first->GetRootWindow();
  if (root->layer() != layer_for_highlighting_->parent())
    root->layer()->Add(layer_for_highlighting_.get());
  else
    root->layer()->StackAtTop(layer_for_highlighting_.get());

  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root);

  gfx::Rect bounds(window_and_bounds.second);
  gfx::Point origin = bounds.origin();
  screen_position_client->ConvertPointFromScreen(root, &origin);
  bounds.set_origin(origin);
  layer_for_highlighting_->SetBounds(bounds);
}

ui_devtools::protocol::Response UIDevToolsDOMAgent::HighlightNode(
    std::unique_ptr<ui_devtools::protocol::DOM::HighlightConfig>
        highlight_config,
    int node_id) {
  if (!layer_for_highlighting_) {
    layer_for_highlighting_.reset(
        new ui::Layer(ui::LayerType::LAYER_SOLID_COLOR));
    layer_for_highlighting_->set_name("HighlightingLayer");
  }

  std::pair<aura::Window*, gfx::Rect> window_and_bounds =
      node_id_to_ui_element_.count(node_id)
          ? node_id_to_ui_element_[node_id]->GetNodeWindowAndBounds()
          : std::make_pair<aura::Window*, gfx::Rect>(nullptr, gfx::Rect());

  if (!window_and_bounds.first) {
    return ui_devtools::protocol::Response::Error("No node found with that id");
  }
  SkColor content_color =
      RGBAToSkColor(highlight_config->getContentColor(nullptr));
  UpdateHighlight(window_and_bounds, content_color);

  if (!layer_for_highlighting_->visible())
    layer_for_highlighting_->SetVisible(true);

  return ui_devtools::protocol::Response::OK();
}

}  // namespace ui_devtools
