// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_ALWAYS_ON_TOP_CONTROLLER_H_
#define ASH_WM_COMMON_ALWAYS_ON_TOP_CONTROLLER_H_

#include <memory>

#include "ash/wm/common/ash_wm_common_export.h"
#include "ash/wm/common/wm_window_observer.h"
#include "base/macros.h"

namespace ash {
class WorkspaceLayoutManager;

// AlwaysOnTopController puts window into proper containers based on its
// 'AlwaysOnTop' property. That is, putting a window into the worskpace
// container if its "AlwaysOnTop" property is false. Otherwise, put it in
// |always_on_top_container_|.
class ASH_WM_COMMON_EXPORT AlwaysOnTopController : public wm::WmWindowObserver {
 public:
  explicit AlwaysOnTopController(wm::WmWindow* viewport);
  ~AlwaysOnTopController() override;

  // Gets container for given |window| based on its "AlwaysOnTop" property.
  wm::WmWindow* GetContainer(wm::WmWindow* window) const;

  WorkspaceLayoutManager* GetLayoutManager() const;

  void SetLayoutManagerForTest(
      std::unique_ptr<WorkspaceLayoutManager> layout_manager);

 private:
  // Overridden from wm::WmWindowObserver:
  void OnWindowTreeChanged(wm::WmWindow* window,
                           const TreeChangeParams& params) override;
  void OnWindowPropertyChanged(wm::WmWindow* window,
                               wm::WmWindowProperty property,
                               intptr_t old) override;
  void OnWindowDestroying(wm::WmWindow* window) override;

  wm::WmWindow* always_on_top_container_;

  DISALLOW_COPY_AND_ASSIGN(AlwaysOnTopController);
};

}  // namepsace ash

#endif  // ASH_WM_COMMON_ALWAYS_ON_TOP_CONTROLLER_H_
