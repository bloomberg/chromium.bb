// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_
#define CHROME_BROWSER_UI_ASH_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_

#include "base/basictypes.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

class AXRootObjWrapper : public views::AXAuraObjWrapper {
 public:
  explicit AXRootObjWrapper(int32 id);
  virtual ~AXRootObjWrapper();

  // Convenience method to check for existence of a child.
  bool HasChild(views::AXAuraObjWrapper* child);

  // views::AXAuraObjWrapper overrides.
  virtual views::AXAuraObjWrapper* GetParent() OVERRIDE;
  virtual void GetChildren(
      std::vector<views::AXAuraObjWrapper*>* out_children) OVERRIDE;
  virtual void Serialize(ui::AXNodeData* out_node_data) OVERRIDE;
  virtual int32 GetID() OVERRIDE;

 private:
  int32 id_;

  DISALLOW_COPY_AND_ASSIGN(AXRootObjWrapper);
};

#endif  // CHROME_BROWSER_UI_ASH_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_
