// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_DEVTOOLS_ASH_DEVTOOLS_CSS_AGENT_H_
#define ASH_COMMON_DEVTOOLS_ASH_DEVTOOLS_CSS_AGENT_H_

#include "ash/common/devtools/ash_devtools_dom_agent.h"
#include "components/ui_devtools/CSS.h"

namespace ash {
namespace devtools {

class ASH_EXPORT AshDevToolsCSSAgent
    : public NON_EXPORTED_BASE(ui::devtools::UiDevToolsBaseAgent<
                               ui::devtools::protocol::CSS::Metainfo>) {
 public:
  explicit AshDevToolsCSSAgent(AshDevToolsDOMAgent* dom_agent);
  ~AshDevToolsCSSAgent() override;

  // CSS::Backend
  ui::devtools::protocol::Response getMatchedStylesForNode(
      int nodeId,
      ui::devtools::protocol::Maybe<ui::devtools::protocol::CSS::CSSStyle>*
          inlineStyle) override;

 private:
  std::unique_ptr<ui::devtools::protocol::CSS::CSSStyle> GetStylesForNode(
      int nodeId);

  AshDevToolsDOMAgent* dom_agent_;

  DISALLOW_COPY_AND_ASSIGN(AshDevToolsCSSAgent);
};

}  // namespace devtools
}  // namespace ash

#endif  // ASH_COMMON_DEVTOOLS_ASH_DEVTOOLS_CSS_AGENT_H_
