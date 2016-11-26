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

std::unique_ptr<CSS::CSSStyle> BuildCSSStyle(
    std::unique_ptr<Array<CSS::CSSProperty>> cssProperties) {
  return CSS::CSSStyle::create()
      .setCssProperties(std::move(cssProperties))
      .setShorthandEntries(Array<std::string>::create())
      .build();
}

std::unique_ptr<CSS::CSSStyle> BuildCSSStyleForBounds(const gfx::Rect& bounds) {
  return BuildCSSStyle(BuildBoundsCSSPropertyArray(bounds));
}

std::unique_ptr<CSS::CSSStyle> BuildStyles(WmWindow* window) {
  return BuildCSSStyleForBounds(window->GetBounds());
}

std::unique_ptr<CSS::CSSStyle> BuildStyles(views::Widget* widget) {
  return BuildCSSStyleForBounds(widget->GetWindowBoundsInScreen());
}

std::unique_ptr<CSS::CSSStyle> BuildStyles(views::View* view) {
  return BuildCSSStyleForBounds(view->bounds());
}

}  // namespace

AshDevToolsCSSAgent::AshDevToolsCSSAgent(AshDevToolsDOMAgent* dom_agent)
    : dom_agent_(dom_agent) {
  DCHECK(dom_agent_);
}

AshDevToolsCSSAgent::~AshDevToolsCSSAgent() {}

ui::devtools::protocol::Response AshDevToolsCSSAgent::getMatchedStylesForNode(
    int node_id,
    ui::devtools::protocol::Maybe<ui::devtools::protocol::CSS::CSSStyle>*
        inlineStyle) {
  *inlineStyle = GetStylesForNode(node_id);
  if (!inlineStyle) {
    return ui::devtools::protocol::Response::Error(
        "Node with that id not found");
  }
  return ui::devtools::protocol::Response::OK();
}

std::unique_ptr<ui::devtools::protocol::CSS::CSSStyle>
AshDevToolsCSSAgent::GetStylesForNode(int node_id) {
  WmWindow* window = dom_agent_->GetWindowFromNodeId(node_id);
  if (window)
    return BuildStyles(window);

  views::Widget* widget = dom_agent_->GetWidgetFromNodeId(node_id);
  if (widget)
    return BuildStyles(widget);

  views::View* view = dom_agent_->GetViewFromNodeId(node_id);
  if (view)
    return BuildStyles(view);

  return nullptr;
}

}  // namespace devtools
}  // namespace ash
