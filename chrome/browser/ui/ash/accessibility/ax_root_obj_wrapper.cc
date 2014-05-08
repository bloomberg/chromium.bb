// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/ax_root_obj_wrapper.h"

#include "ash/shell.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/aura/window.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"

AXRootObjWrapper::AXRootObjWrapper(int32 id) : id_(id) {
}

AXRootObjWrapper::~AXRootObjWrapper() {}

bool AXRootObjWrapper::HasChild(views::AXAuraObjWrapper* child) {
  std::vector<views::AXAuraObjWrapper*> children;
  GetChildren(&children);
  return std::find(children.begin(), children.end(), child) != children.end();
}

views::AXAuraObjWrapper* AXRootObjWrapper::GetParent() {
  return NULL;
}

void AXRootObjWrapper::GetChildren(
    std::vector<views::AXAuraObjWrapper*>* out_children) {
  if (!ash::Shell::HasInstance())
    return;

  // Only on ash is there a notion of a root with children.
  aura::Window::Windows children =
      ash::Shell::GetInstance()->GetAllRootWindows();
  for (size_t i = 0; i < children.size(); ++i) {
    out_children->push_back(
        views::AXAuraObjCache::GetInstance()->GetOrCreate(children[i]));
  }
}

void AXRootObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  out_node_data->id = id_;
  out_node_data->role = ui::AX_ROLE_DESKTOP;
  // TODO(dtseng): Apply a richer set of states.
  out_node_data->state = 0;
}

int32 AXRootObjWrapper::GetID() {
  return id_;
}
