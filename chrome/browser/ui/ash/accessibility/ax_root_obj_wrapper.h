// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_
#define CHROME_BROWSER_UI_ASH_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_

#include <string>

#include "base/basictypes.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

namespace aura {
class Window;
}  // namespace aura

class AXRootObjWrapper : public views::AXAuraObjWrapper {
 public:
  explicit AXRootObjWrapper(int32 id);
  ~AXRootObjWrapper() override;

  // Returns an AXAuraObjWrapper for an alert window with title set to |text|.
  views::AXAuraObjWrapper* GetAlertForText(const std::string& text);

  // Convenience method to check for existence of a child.
  bool HasChild(views::AXAuraObjWrapper* child);

  // views::AXAuraObjWrapper overrides.
  views::AXAuraObjWrapper* GetParent() override;
  void GetChildren(
      std::vector<views::AXAuraObjWrapper*>* out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  int32 GetID() override;

 private:
  int32 id_;

  aura::Window* alert_window_;

  DISALLOW_COPY_AND_ASSIGN(AXRootObjWrapper);
};

#endif  // CHROME_BROWSER_UI_ASH_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_
