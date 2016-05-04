// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_DELEGATE_H_
#define ASH_WM_COMMON_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_DELEGATE_H_

#include "ash/wm/common/ash_wm_common_export.h"

namespace ash {
namespace wm {

class ASH_WM_COMMON_EXPORT WorkspaceLayoutManagerDelegate {
 public:
  virtual ~WorkspaceLayoutManagerDelegate() {}

  // Called when the visibility of the shelf needs to be updated.
  virtual void UpdateShelfVisibility() = 0;

  // Called when the fullscreen state has changed to |is_fullscreen|.
  virtual void OnFullscreenStateChanged(bool is_fullscreen) = 0;
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_DELEGATE_H_
