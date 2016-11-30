// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/devtools/ash_devtools_css_agent.h"

#include "ash/common/wm_window.h"

namespace ash {
namespace devtools {

namespace {
using namespace ui::devtools::protocol;

std::unique_ptr<CSS::CSSProperty> BuildCSSProperty(const std::string& name,
                                                   int value) {
  return CSS::CSSProperty::create()
      .setName(name)
      .setValue(base::IntToString(value))
      .build();
}

std::unique_ptr<Array<CSS::CSSProperty>> BuildBoundsCSSPropertyArray(
    const gfx::Rect& bounds) {
  auto cssProperties = Array<CSS::CSSProperty>::create();
  cssProperties->addItem(BuildCSSProperty("height", bounds.height()));
  cssProperties->addItem(BuildCSSProperty("width", bounds.width()));
  cssProperties->addItem(BuildCSSProperty("x", bounds.x()));
  cssProperties->addItem(BuildCSSProperty("y", bounds.y()));
  return cssProperties;
}

std::unique_ptr<CSS::CSSStyle> BuildCSSStyle(int node_id,
                                             const gfx::Rect& bounds) {
  return CSS::CSSStyle::create()
      .setCssProperties(BuildBoundsCSSPropertyArray(bounds))
      .setShorthandEntries(Array<std::string>::create())
      .build();
}

}  // namespace

AshDevToolsCSSAgent::AshDevToolsCSSAgent(AshDevToolsDOMAgent* dom_agent)
    : dom_agent_(dom_agent) {
  DCHECK(dom_agent_);
}

AshDevToolsCSSAgent::~AshDevToolsCSSAgent() {
  disable();
}

ui::devtools::protocol::Response AshDevToolsCSSAgent::enable() {
  dom_agent_->AddObserver(this);
  return ui::devtools::protocol::Response::OK();
}

ui::devtools::protocol::Response AshDevToolsCSSAgent::disable() {
  dom_agent_->RemoveObserver(this);
  return ui::devtools::protocol::Response::OK();
}

ui::devtools::protocol::Response AshDevToolsCSSAgent::getMatchedStylesForNode(
    int node_id,
    ui::devtools::protocol::Maybe<ui::devtools::protocol::CSS::CSSStyle>*
        inline_style) {
  *inline_style = GetStylesForNode(node_id);
  if (!inline_style) {
    return ui::devtools::protocol::Response::Error(
        "Node with that id not found");
  }
  return ui::devtools::protocol::Response::OK();
}

void AshDevToolsCSSAgent::OnWindowBoundsChanged(WmWindow* window) {
  InvalidateStyleSheet(dom_agent_->GetNodeIdFromWindow(window));
}

void AshDevToolsCSSAgent::OnWidgetBoundsChanged(views::Widget* widget) {
  InvalidateStyleSheet(dom_agent_->GetNodeIdFromWidget(widget));
}

void AshDevToolsCSSAgent::OnViewBoundsChanged(views::View* view) {
  InvalidateStyleSheet(dom_agent_->GetNodeIdFromView(view));
}

std::unique_ptr<ui::devtools::protocol::CSS::CSSStyle>
AshDevToolsCSSAgent::GetStylesForNode(int node_id) {
  WmWindow* window = dom_agent_->GetWindowFromNodeId(node_id);
  if (window)
    return BuildCSSStyle(node_id, window->GetBounds());

  views::Widget* widget = dom_agent_->GetWidgetFromNodeId(node_id);
  if (widget)
    return BuildCSSStyle(node_id, widget->GetWindowBoundsInScreen());

  views::View* view = dom_agent_->GetViewFromNodeId(node_id);
  if (view)
    return BuildCSSStyle(node_id, view->bounds());

  return nullptr;
}

void AshDevToolsCSSAgent::InvalidateStyleSheet(int node_id) {
  // The stylesheetId for each node is equivalent to its node_id (as a string).
  frontend()->styleSheetChanged(base::IntToString(node_id));
}

}  // namespace devtools
}  // namespace ash
