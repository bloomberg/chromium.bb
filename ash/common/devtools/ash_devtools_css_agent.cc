// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/devtools/ash_devtools_css_agent.h"

#include "ash/common/wm_lookup.h"
#include "ash/common/wm_window.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace ash {
namespace devtools {

namespace {
using namespace ui::devtools::protocol;

const char kHeight[] = "height";
const char kWidth[] = "width";
const char kX[] = "x";
const char kY[] = "y";

std::unique_ptr<CSS::SourceRange> BuildDefaultSourceRange() {
  // These tell the frontend where in the stylesheet a certain style
  // is located. Since we don't have stylesheets, this is all 0.
  // We need this because CSS fields are not editable unless
  // the range is provided.
  return CSS::SourceRange::create()
      .setStartLine(0)
      .setEndLine(0)
      .setStartColumn(0)
      .setEndColumn(0)
      .build();
}

std::unique_ptr<CSS::CSSProperty> BuildCSSProperty(const std::string& name,
                                                   int value) {
  return CSS::CSSProperty::create()
      .setRange(BuildDefaultSourceRange())
      .setName(name)
      .setValue(base::IntToString(value))
      .build();
}

std::unique_ptr<Array<CSS::CSSProperty>> BuildBoundsCSSPropertyArray(
    const gfx::Rect& bounds) {
  auto cssProperties = Array<CSS::CSSProperty>::create();
  cssProperties->addItem(BuildCSSProperty(kHeight, bounds.height()));
  cssProperties->addItem(BuildCSSProperty(kWidth, bounds.width()));
  cssProperties->addItem(BuildCSSProperty(kX, bounds.x()));
  cssProperties->addItem(BuildCSSProperty(kY, bounds.y()));
  return cssProperties;
}

std::unique_ptr<CSS::CSSStyle> BuildCSSStyle(int node_id,
                                             const gfx::Rect& bounds) {
  return CSS::CSSStyle::create()
      .setRange(BuildDefaultSourceRange())
      .setStyleSheetId(base::IntToString(node_id))
      .setCssProperties(BuildBoundsCSSPropertyArray(bounds))
      .setShorthandEntries(Array<std::string>::create())
      .build();
}

Response ParseBounds(const std::string& style_text, gfx::Rect& bounds) {
  std::vector<std::string> tokens = base::SplitString(
      style_text, ":;", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (size_t i = 0; i < tokens.size() - 1; i += 2) {
    const std::string& property = tokens.at(i);
    int value;
    if (!base::StringToInt(tokens.at(i + 1), &value))
      return Response::Error("Unable to parse value for property=" + property);

    if (property == kHeight)
      bounds.set_height(std::max(0, value));
    else if (property == kWidth)
      bounds.set_width(std::max(0, value));
    else if (property == kX)
      bounds.set_x(value);
    else if (property == kY)
      bounds.set_y(value);
    else
      return Response::Error("Unsupported property=" + property);
  }
  return Response::OK();
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

ui::devtools::protocol::Response AshDevToolsCSSAgent::setStyleTexts(
    std::unique_ptr<ui::devtools::protocol::Array<
        ui::devtools::protocol::CSS::StyleDeclarationEdit>> edits,
    std::unique_ptr<
        ui::devtools::protocol::Array<ui::devtools::protocol::CSS::CSSStyle>>*
        result) {
  std::unique_ptr<
      ui::devtools::protocol::Array<ui::devtools::protocol::CSS::CSSStyle>>
      updated_styles = ui::devtools::protocol::Array<
          ui::devtools::protocol::CSS::CSSStyle>::create();
  for (size_t i = 0; i < edits->length(); i++) {
    auto* edit = edits->get(i);
    int node_id;
    if (!base::StringToInt(edit->getStyleSheetId(), &node_id))
      return ui::devtools::protocol::Response::Error("Invalid node id");

    gfx::Rect updated_bounds;
    if (!GetBoundsForNodeId(node_id, &updated_bounds)) {
      return ui::devtools::protocol::Response::Error(
          "No node found with that id");
    }

    ui::devtools::protocol::Response response(
        ParseBounds(edit->getText(), updated_bounds));
    if (!response.isSuccess())
      return response;

    updated_styles->addItem(BuildCSSStyle(node_id, updated_bounds));
    if (!UpdateBounds(node_id, updated_bounds)) {
      return ui::devtools::protocol::Response::Error(
          "No node found with that id");
    }
  }

  *result = std::move(updated_styles);
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
  gfx::Rect bounds;
  return GetBoundsForNodeId(node_id, &bounds) ? BuildCSSStyle(node_id, bounds)
                                              : nullptr;
}

void AshDevToolsCSSAgent::InvalidateStyleSheet(int node_id) {
  // The stylesheetId for each node is equivalent to its node_id (as a string).
  frontend()->styleSheetChanged(base::IntToString(node_id));
}

bool AshDevToolsCSSAgent::GetBoundsForNodeId(int node_id, gfx::Rect* bounds) {
  WmWindow* window = dom_agent_->GetWindowFromNodeId(node_id);
  if (window) {
    *bounds = window->GetBounds();
    return true;
  }

  views::Widget* widget = dom_agent_->GetWidgetFromNodeId(node_id);
  if (widget) {
    *bounds = widget->GetRestoredBounds();
    return true;
  }

  views::View* view = dom_agent_->GetViewFromNodeId(node_id);
  if (view) {
    *bounds = view->bounds();
    return true;
  }

  return false;
}

bool AshDevToolsCSSAgent::UpdateBounds(int node_id, const gfx::Rect& bounds) {
  WmWindow* window = dom_agent_->GetWindowFromNodeId(node_id);
  if (window) {
    window->SetBounds(bounds);
    return true;
  }

  views::Widget* widget = dom_agent_->GetWidgetFromNodeId(node_id);
  if (widget) {
    widget->SetBounds(bounds);
    return true;
  }

  views::View* view = dom_agent_->GetViewFromNodeId(node_id);
  if (view) {
    view->SetBoundsRect(bounds);
    return true;
  }

  return false;
}

}  // namespace devtools
}  // namespace ash
