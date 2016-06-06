// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_DELEGATE_H_
#define ASH_COMMON_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_DELEGATE_H_

#include "ash/ash_export.h"

namespace ash {
namespace wm {

class ASH_EXPORT WorkspaceLayoutManagerDelegate {
 public:
  virtual ~WorkspaceLayoutManagerDelegate() {}

  // Called when the visibility of the shelf needs to be updated.
  // TODO(jamescook): Eliminate this and use WmShelf directly.
  virtual void UpdateShelfVisibility() = 0;

  // Called when the fullscreen state has changed to |is_fullscreen|.
  virtual void OnFullscreenStateChanged(bool is_fullscreen) = 0;
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_COMMON_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_DELEGATE_H_
