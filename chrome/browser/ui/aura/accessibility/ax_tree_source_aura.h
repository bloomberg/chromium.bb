// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AX_TREE_SOURCE_AURA_H_
#define CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AX_TREE_SOURCE_AURA_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/aura/accessibility/ax_root_obj_wrapper.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_source.h"

namespace views {
class AXAuraObjWrapper;
}  // namespace views

// This class exposes the views hierarchy as an accessibility tree permitting
// use with other accessibility classes.
class AXTreeSourceAura
    : public ui::AXTreeSource<views::AXAuraObjWrapper*,
                              ui::AXNodeData,
                              ui::AXTreeData> {
 public:
  AXTreeSourceAura();
  ~AXTreeSourceAura() override;

  // A set of actions invoked on an Aura view.
  void DoDefault(int32 id);
  void Focus(int32 id);
  void MakeVisible(int32 id);
  void SetSelection(int32 id, int32 start, int32 end);
  void ShowContextMenu(int32 id);

  // AXTreeSource implementation.
  ui::AXTreeData GetTreeData() const override;
  views::AXAuraObjWrapper* GetRoot() const override;
  views::AXAuraObjWrapper* GetFromId(int32 id) const override;
  int32 GetId(views::AXAuraObjWrapper* node) const override;
  void GetChildren(
      views::AXAuraObjWrapper* node,
      std::vector<views::AXAuraObjWrapper*>* out_children) const override;
  views::AXAuraObjWrapper* GetParent(
      views::AXAuraObjWrapper* node) const override;
  bool IsValid(views::AXAuraObjWrapper* node) const override;
  bool IsEqual(views::AXAuraObjWrapper* node1,
               views::AXAuraObjWrapper* node2) const override;
  views::AXAuraObjWrapper* GetNull() const override;
  void SerializeNode(views::AXAuraObjWrapper* node,
                     ui::AXNodeData* out_data) const override;

  // Useful for debugging.
  std::string ToString(views::AXAuraObjWrapper* root, std::string prefix);

 private:
  scoped_ptr<AXRootObjWrapper> root_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceAura);
};

#endif  // CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AX_TREE_SOURCE_AURA_H_
