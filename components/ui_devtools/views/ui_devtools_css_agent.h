// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_UI_DEVTOOLS_CSS_AGENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_UI_DEVTOOLS_CSS_AGENT_H_

#include "base/macros.h"
#include "components/ui_devtools/CSS.h"
#include "components/ui_devtools/views/ui_devtools_dom_agent.h"

namespace ui_devtools {

class UIDevToolsCSSAgent : public ui_devtools::UiDevToolsBaseAgent<
                               ui_devtools::protocol::CSS::Metainfo>,
                           public UIDevToolsDOMAgentObserver {
 public:
  explicit UIDevToolsCSSAgent(UIDevToolsDOMAgent* dom_agent);
  ~UIDevToolsCSSAgent() override;

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
  void OnNodeBoundsChanged(int node_id) override;

 private:
  std::unique_ptr<ui_devtools::protocol::CSS::CSSStyle> GetStylesForNode(
      int node_id);
  void InvalidateStyleSheet(int node_id);
  bool GetPropertiesForNodeId(int node_id, gfx::Rect* bounds, bool* visible);
  bool SetPropertiesForNodeId(int node_id,
                              const gfx::Rect& bounds,
                              bool visible);
  UIDevToolsDOMAgent* dom_agent_;

  DISALLOW_COPY_AND_ASSIGN(UIDevToolsCSSAgent);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_UI_DEVTOOLS_CSS_AGENT_H_
