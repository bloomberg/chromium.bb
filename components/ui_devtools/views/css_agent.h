// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_CSS_AGENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_CSS_AGENT_H_

#include "base/macros.h"
#include "components/ui_devtools/CSS.h"
#include "components/ui_devtools/views/ui_devtools_dom_agent.h"

namespace ui_devtools {

class UIElement;

class CSSAgent : public ui_devtools::UiDevToolsBaseAgent<
                     ui_devtools::protocol::CSS::Metainfo>,
                 public UIDevToolsDOMAgentObserver {
 public:
  explicit CSSAgent(UIDevToolsDOMAgent* dom_agent);
  ~CSSAgent() override;

  // CSS::Backend:
  ui_devtools::protocol::Response enable() override;
  ui_devtools::protocol::Response disable() override;
  ui_devtools::protocol::Response getMatchedStylesForNode(
      int node_id,
      ui_devtools::protocol::Maybe<ui_devtools::protocol::CSS::CSSStyle>*
          inline_style) override;
  ui_devtools::protocol::Response setStyleTexts(
      std::unique_ptr<ui_devtools::protocol::Array<
          ui_devtools::protocol::CSS::StyleDeclarationEdit>> edits,
      std::unique_ptr<
          ui_devtools::protocol::Array<ui_devtools::protocol::CSS::CSSStyle>>*
          result) override;

  // UIDevToolsDOMAgentObserver:
  void OnElementBoundsChanged(UIElement* ui_element) override;

 private:
  std::unique_ptr<ui_devtools::protocol::CSS::CSSStyle> GetStylesForUIElement(
      UIElement* ui_element);
  void InvalidateStyleSheet(UIElement* ui_element);
  bool GetPropertiesForUIElement(UIElement* ui_element,
                                 gfx::Rect* bounds,
                                 bool* visible);
  bool SetPropertiesForUIElement(UIElement* ui_element,
                                 const gfx::Rect& bounds,
                                 bool visible);
  UIDevToolsDOMAgent* const dom_agent_;

  DISALLOW_COPY_AND_ASSIGN(CSSAgent);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_CSS_AGENT_H_
