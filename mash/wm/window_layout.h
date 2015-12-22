// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_WINDOW_LAYOUT_H_
#define MASH_WM_WINDOW_LAYOUT_H_

#include <stdint.h>

#include "base/macros.h"
#include "mash/wm/layout_manager.h"

namespace mash {
namespace wm {

// Responsible for layout of top level windows.
class WindowLayout : public LayoutManager {
 public:
  explicit WindowLayout(mus::Window* owner);
  ~WindowLayout() override;

 private:
  // Overridden from LayoutManager:
  void LayoutWindow(mus::Window* window) override;
  void OnWindowSharedPropertyChanged(
      mus::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override;

  void FitToContainer(mus::Window* window);
  void CenterWindow(mus::Window* window, const gfx::Size& preferred_size);

  DISALLOW_COPY_AND_ASSIGN(WindowLayout);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_WINDOW_LAYOUT_H_
