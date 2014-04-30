// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/ax_tree_source_views.h"

#include <vector>

#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

using views::AXAuraObjCache;
using views::AXAuraObjWrapper;

AXTreeSourceViews::AXTreeSourceViews() {
  root_.reset(
      new AXRootObjWrapper(AXAuraObjCache::GetInstance()->GetNextID()));
}

AXTreeSourceViews::~AXTreeSourceViews() {
  root_.reset();
}

AXAuraObjWrapper* AXTreeSourceViews::GetRoot() const {
  return root_.get();
}

AXAuraObjWrapper* AXTreeSourceViews::GetFromId(int32 id) const {
  if (id == root_->GetID())
    return root_.get();
  return AXAuraObjCache::GetInstance()->Get(id);
}

int32 AXTreeSourceViews::GetId(AXAuraObjWrapper* node) const {
  return node->GetID();
}

void AXTreeSourceViews::GetChildren(AXAuraObjWrapper* node,
    std::vector<AXAuraObjWrapper*>* out_children) const {
  node->GetChildren(out_children);
}

AXAuraObjWrapper* AXTreeSourceViews::GetParent(AXAuraObjWrapper* node) const {
  AXAuraObjWrapper* parent = node->GetParent();
  if (!parent && root_->HasChild(node))
    parent = root_.get();
  return parent;
}

bool AXTreeSourceViews::IsValid(AXAuraObjWrapper* node) const {
  return node && node->GetID() != -1;
}

bool AXTreeSourceViews::IsEqual(AXAuraObjWrapper* node1,
                                AXAuraObjWrapper* node2) const {
  if (!node1 || !node2)
    return false;

  return node1->GetID() == node2->GetID() && node1->GetID() != -1;
}

AXAuraObjWrapper* AXTreeSourceViews::GetNull() const {
  return NULL;
}

void AXTreeSourceViews::SerializeNode(
    AXAuraObjWrapper* node, ui::AXNodeData* out_data) const {
  node->Serialize(out_data);
}

std::string AXTreeSourceViews::ToString(
    AXAuraObjWrapper* root, std::string prefix) {
  ui::AXNodeData data;
  root->Serialize(&data);
  std::string output = prefix + data.ToString() + '\n';

  std::vector<AXAuraObjWrapper*> children;
  root->GetChildren(&children);

  prefix += prefix[0];
  for (size_t i = 0; i < children.size(); ++i)
    output += ToString(children[i], prefix);

  return output;
}
