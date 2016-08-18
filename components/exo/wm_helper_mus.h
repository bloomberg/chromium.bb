// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_MUS_H_
#define COMPONENTS_EXO_WM_HELPER_MUS_H_

#include "base/macros.h"
#include "components/exo/wm_helper.h"

namespace exo {

// A helper class for accessing WindowManager related features.
class WMHelperMus : public WMHelper {
 public:
  WMHelperMus();
  ~WMHelperMus() override;

  // Overriden from WMHelper:
  const ash::DisplayInfo GetDisplayInfo(int64_t display_id) const override;
  aura::Window* GetContainer(int container_id) override;
  aura::Window* GetActiveWindow() const override;
  aura::Window* GetFocusedWindow() const override;
  ui::CursorSetType GetCursorSet() const override;
  void AddPreTargetHandler(ui::EventHandler* handler) override;
  void PrependPreTargetHandler(ui::EventHandler* handler) override;
  void RemovePreTargetHandler(ui::EventHandler* handler) override;
  void AddPostTargetHandler(ui::EventHandler* handler) override;
  void RemovePostTargetHandler(ui::EventHandler* handler) override;
  bool IsMaximizeModeWindowManagerEnabled() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WMHelperMus);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_H_
