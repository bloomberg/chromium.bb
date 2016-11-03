// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_DEVTOOLS_ASH_DEVTOOLS_DOM_AGENT_H_
#define ASH_COMMON_DEVTOOLS_ASH_DEVTOOLS_DOM_AGENT_H_

#include "ash/common/wm_shell.h"
#include "base/compiler_specific.h"
#include "components/ui_devtools/DOM.h"
#include "components/ui_devtools/devtools_base_agent.h"

namespace ash {
namespace devtools {

class ASH_EXPORT AshDevToolsDOMAgent
    : public NON_EXPORTED_BASE(ui::devtools::UiDevToolsBaseAgent<
                               ui::devtools::protocol::DOM::Metainfo>) {
 public:
  explicit AshDevToolsDOMAgent(ash::WmShell* shell);
  ~AshDevToolsDOMAgent() override;

  // DOM::Backend
  ui::devtools::protocol::Response enable() override;
  ui::devtools::protocol::Response disable() override;
  ui::devtools::protocol::Response getDocument(
      std::unique_ptr<ui::devtools::protocol::DOM::Node>* out_root) override;

 private:
  std::unique_ptr<ui::devtools::protocol::DOM::Node> BuildInitialTree();

  ash::WmShell* shell_;

  DISALLOW_COPY_AND_ASSIGN(AshDevToolsDOMAgent);
};

}  // namespace devtools
}  // namespace ash

#endif  // ASH_COMMON_DEVTOOLS_ASH_DEVTOOLS_DOM_AGENT_H_
