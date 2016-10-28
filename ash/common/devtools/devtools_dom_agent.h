// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_DEVTOOLS_DEVTOOLS_DOM_AGENT_H_
#define ASH_COMMON_DEVTOOLS_DEVTOOLS_DOM_AGENT_H_

#include "ash/common/wm_shell.h"
#include "components/ui_devtools/DOM.h"

namespace ash {
namespace devtools {

class AshDevToolsDOMAgent : public ui::devtools::protocol::DOM::Backend {
 public:
  explicit AshDevToolsDOMAgent(ash::WmShell* shell);
  ~AshDevToolsDOMAgent() override;

 private:
  std::unique_ptr<ui::devtools::protocol::DOM::Node> BuildInitialTree();

  // DOM::Backend
  void enable(ui::devtools::protocol::ErrorString* error) override {}
  void disable(ui::devtools::protocol::ErrorString* error) override {}
  void getDocument(
      ui::devtools::protocol::ErrorString* error,
      std::unique_ptr<ui::devtools::protocol::DOM::Node>* out_root) override;

  ash::WmShell* shell_;

  DISALLOW_COPY_AND_ASSIGN(AshDevToolsDOMAgent);
};

}  // namespace devtools
}  // namespace ash

#endif  // ASH_COMMON_DEVTOOLS_DEVTOOLS_DOM_AGENT_H_
