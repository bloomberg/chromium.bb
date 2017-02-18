// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_WINDOW_USER_DATA_H_
#define ASH_COMMON_WM_WINDOW_USER_DATA_H_

#include <map>
#include <memory>
#include <utility>

#include "ash/common/wm_window.h"
#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

// WmWindowUserData provides a way to associate arbitrary objects with a
// WmWindow. WmWindowUserData owns the data, deleting it either when
// WmWindowUserData is deleted, or when the window the data is associated with
// is destroyed (from aura::WindowObserver::OnWindowDestroying()).
template <typename UserData>
class WmWindowUserData : public aura::WindowObserver {
 public:
  WmWindowUserData() {}

  ~WmWindowUserData() override { clear(); }

  void clear() {
    for (auto& pair : window_to_data_)
      pair.first->aura_window()->RemoveObserver(this);
    window_to_data_.clear();
  }

  // Sets the data associated with window. This destroys any existing data.
  // |data| may be null.
  void Set(WmWindow* window, std::unique_ptr<UserData> data) {
    if (!data) {
      if (window_to_data_.erase(window))
        window->aura_window()->RemoveObserver(this);
      return;
    }
    if (window_to_data_.count(window) == 0u)
      window->aura_window()->AddObserver(this);
    window_to_data_[window] = std::move(data);
  }

  // Returns the data associated with the window, or null if none set. The
  // returned object is owned by WmWindowUserData.
  UserData* Get(WmWindow* window) {
    auto it = window_to_data_.find(window);
    return it == window_to_data_.end() ? nullptr : it->second.get();
  }

  // Returns the set of windows with data associated with them.
  std::set<WmWindow*> GetWindows() {
    std::set<WmWindow*> windows;
    for (auto& pair : window_to_data_)
      windows.insert(pair.first);
    return windows;
  }

 private:
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
    window_to_data_.erase(WmWindow::Get(window));
  }

  std::map<WmWindow*, std::unique_ptr<UserData>> window_to_data_;

  DISALLOW_COPY_AND_ASSIGN(WmWindowUserData);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_WINDOW_USER_DATA_H_
