// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_DEVTOOLS_ASH_DEVTOOLS_CSS_AGENT_H_
#define ASH_COMMON_DEVTOOLS_ASH_DEVTOOLS_CSS_AGENT_H_

#include "ash/common/devtools/ash_devtools_dom_agent.h"
#include "components/ui_devtools/CSS.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
namespace devtools {

class ASH_EXPORT AshDevToolsCSSAgent
    : public NON_EXPORTED_BASE(ui::devtools::UiDevToolsBaseAgent<
                               ui::devtools::protocol::CSS::Metainfo>),
      public AshDevToolsDOMAgentObserver {
 public:
  explicit AshDevToolsCSSAgent(AshDevToolsDOMAgent* dom_agent);
  ~AshDevToolsCSSAgent() override;

  // CSS::Backend
  ui::devtools::protocol::Response enable() override;
  ui::devtools::protocol::Response disable() override;
  ui::devtools::protocol::Response getMatchedStylesForNode(
      int node_id,
      ui::devtools::protocol::Maybe<ui::devtools::protocol::CSS::CSSStyle>*
          inline_style) override;
  ui::devtools::protocol::Response setStyleTexts(
      std::unique_ptr<ui::devtools::protocol::Array<
          ui::devtools::protocol::CSS::StyleDeclarationEdit>> edits,
      std::unique_ptr<
          ui::devtools::protocol::Array<ui::devtools::protocol::CSS::CSSStyle>>*
          result) override;

  // AshDevToolsDOMAgentObserver
  void OnWindowBoundsChanged(WmWindow* window) override;
  void OnWidgetBoundsChanged(views::Widget* widget) override;
  void OnViewBoundsChanged(views::View* view) override;

 private:
  std::unique_ptr<ui::devtools::protocol::CSS::CSSStyle> GetStylesForNode(
      int node_id);
  void InvalidateStyleSheet(int node_id);
  bool GetBoundsForNodeId(int node_id, gfx::Rect* bounds);
  bool UpdateBounds(int node_id, const gfx::Rect& bounds);

  AshDevToolsDOMAgent* dom_agent_;

  DISALLOW_COPY_AND_ASSIGN(AshDevToolsCSSAgent);
};

}  // namespace devtools
}  // namespace ash

#endif  // ASH_COMMON_DEVTOOLS_ASH_DEVTOOLS_CSS_AGENT_H_
