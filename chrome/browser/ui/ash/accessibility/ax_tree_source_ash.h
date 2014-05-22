// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCESSIBILITY_AX_TREE_SOURCE_ASH_H_
#define CHROME_BROWSER_UI_ASH_ACCESSIBILITY_AX_TREE_SOURCE_ASH_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/ash/accessibility/ax_root_obj_wrapper.h"
#include "ui/accessibility/ax_tree_source.h"

namespace views {
class AXAuraObjWrapper;
}  // namespace views

// This class exposes the views hierarchy as an accessibility tree permitting
// use with other accessibility classes.
class AXTreeSourceAsh
    : public ui::AXTreeSource<views::AXAuraObjWrapper*> {
 public:
  AXTreeSourceAsh();
  virtual ~AXTreeSourceAsh();

  // A set of actions invoked on an Aura view.
  void DoDefault(int32 id);
  void Focus(int32 id);
  void MakeVisible(int32 id);
  void SetSelection(int32 id, int32 start, int32 end);

  // AXTreeSource implementation.
  virtual views::AXAuraObjWrapper* GetRoot() const OVERRIDE;
  virtual views::AXAuraObjWrapper* GetFromId(int32 id) const OVERRIDE;
  virtual int32 GetId(views::AXAuraObjWrapper* node) const OVERRIDE;
  virtual void GetChildren(views::AXAuraObjWrapper* node,
      std::vector<views::AXAuraObjWrapper*>* out_children) const OVERRIDE;
  virtual views::AXAuraObjWrapper* GetParent(
      views::AXAuraObjWrapper* node) const OVERRIDE;
  virtual bool IsValid(views::AXAuraObjWrapper* node) const OVERRIDE;
  virtual bool IsEqual(views::AXAuraObjWrapper* node1,
                       views::AXAuraObjWrapper* node2) const OVERRIDE;
  virtual views::AXAuraObjWrapper* GetNull() const OVERRIDE;
  virtual void SerializeNode(
      views::AXAuraObjWrapper* node, ui::AXNodeData* out_data) const OVERRIDE;

  // Useful for debugging.
  std::string ToString(views::AXAuraObjWrapper* root, std::string prefix);

 private:
  scoped_ptr<AXRootObjWrapper> root_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceAsh);
};

#endif  // CHROME_BROWSER_UI_ASH_ACCESSIBILITY_AX_TREE_SOURCE_ASH_H_
