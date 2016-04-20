// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/window_parenting_utils.h"

#include "ash/wm/common/wm_window.h"

namespace ash {
namespace wm {

void ReparentChildWithTransientChildren(WmWindow* child,
                                        WmWindow* old_parent,
                                        WmWindow* new_parent) {
  if (child->GetParent() == old_parent)
    new_parent->AddChild(child);
  ReparentTransientChildrenOfChild(child, old_parent, new_parent);
}

void ReparentTransientChildrenOfChild(WmWindow* child,
                                      WmWindow* old_parent,
                                      WmWindow* new_parent) {
  for (WmWindow* transient_child : child->GetTransientChildren())
    ReparentChildWithTransientChildren(transient_child, old_parent, new_parent);
}

}  // namespace wm
}  // namespace ash
