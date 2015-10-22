// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/ax_tree_source_aura.h"

#include <vector>

#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_view_obj_wrapper.h"
#include "ui/views/controls/webview/webview.h"

using views::AXAuraObjCache;
using views::AXAuraObjWrapper;

AXTreeSourceAura::AXTreeSourceAura() {
  root_.reset(new AXRootObjWrapper(AXAuraObjCache::GetInstance()->GetNextID()));
}

AXTreeSourceAura::~AXTreeSourceAura() {
  root_.reset();
}

void AXTreeSourceAura::DoDefault(int32 id) {
  AXAuraObjWrapper* obj = AXAuraObjCache::GetInstance()->Get(id);
  if (obj)
    obj->DoDefault();
}

void AXTreeSourceAura::Focus(int32 id) {
  AXAuraObjWrapper* obj = AXAuraObjCache::GetInstance()->Get(id);
  if (obj)
    obj->Focus();
}

void AXTreeSourceAura::MakeVisible(int32 id) {
  AXAuraObjWrapper* obj = AXAuraObjCache::GetInstance()->Get(id);
  if (obj)
    obj->MakeVisible();
}

void AXTreeSourceAura::SetSelection(int32 id, int32 start, int32 end) {
  AXAuraObjWrapper* obj = AXAuraObjCache::GetInstance()->Get(id);
  if (obj)
    obj->SetSelection(start, end);
}

void AXTreeSourceAura::ShowContextMenu(int32 id) {
  AXAuraObjWrapper* obj = AXAuraObjCache::GetInstance()->Get(id);
  if (obj)
    obj->ShowContextMenu();
}

ui::AXTreeData AXTreeSourceAura::GetTreeData() const {
  ui::AXTreeData tree_data;
  tree_data.tree_id = 0;
  tree_data.loaded = true;
  tree_data.loading_progress = 1.0;
  return tree_data;
}

AXAuraObjWrapper* AXTreeSourceAura::GetRoot() const {
  return root_.get();
}

AXAuraObjWrapper* AXTreeSourceAura::GetFromId(int32 id) const {
  if (id == root_->GetID())
    return root_.get();
  return AXAuraObjCache::GetInstance()->Get(id);
}

int32 AXTreeSourceAura::GetId(AXAuraObjWrapper* node) const {
  return node->GetID();
}

void AXTreeSourceAura::GetChildren(
    AXAuraObjWrapper* node,
    std::vector<AXAuraObjWrapper*>* out_children) const {
  node->GetChildren(out_children);
}

AXAuraObjWrapper* AXTreeSourceAura::GetParent(AXAuraObjWrapper* node) const {
  AXAuraObjWrapper* parent = node->GetParent();
  if (!parent && node->GetID() != root_->GetID())
    parent = root_.get();
  return parent;
}

bool AXTreeSourceAura::IsValid(AXAuraObjWrapper* node) const {
  return node && node->GetID() != -1;
}

bool AXTreeSourceAura::IsEqual(AXAuraObjWrapper* node1,
                               AXAuraObjWrapper* node2) const {
  if (!node1 || !node2)
    return false;

  return node1->GetID() == node2->GetID() && node1->GetID() != -1;
}

AXAuraObjWrapper* AXTreeSourceAura::GetNull() const {
  return NULL;
}

void AXTreeSourceAura::SerializeNode(AXAuraObjWrapper* node,
                                     ui::AXNodeData* out_data) const {
  node->Serialize(out_data);

  if (out_data->role == ui::AX_ROLE_WEB_VIEW) {
    views::View* view = static_cast<views::AXViewObjWrapper*>(node)->view();
    content::WebContents* contents =
        static_cast<views::WebView*>(view)->GetWebContents();
    content::RenderFrameHost* rfh = contents->GetMainFrame();
    if (rfh) {
      int ax_tree_id = rfh->GetAXTreeID();
      out_data->AddIntAttribute(ui::AX_ATTR_CHILD_TREE_ID, ax_tree_id);
    }
  }
}

std::string AXTreeSourceAura::ToString(AXAuraObjWrapper* root,
                                       std::string prefix) {
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
