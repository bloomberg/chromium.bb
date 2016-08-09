// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_WINDOW_MUS_TEST_API_H_
#define ASH_MUS_BRIDGE_WM_WINDOW_MUS_TEST_API_H_

#include "ash/mus/bridge/wm_window_mus.h"

namespace ash {
namespace mus {

class WmWindowMusTestApi {
 public:
  explicit WmWindowMusTestApi(WmWindow* window)
      : WmWindowMusTestApi(WmWindowMus::AsWmWindowMus(window)) {}
  explicit WmWindowMusTestApi(WmWindowMus* window) : window_(window) {}
  ~WmWindowMusTestApi() {}

  void set_use_empty_minimum_size(bool value) {
    window_->use_empty_minimum_size_for_testing_ = true;
  }

 private:
  WmWindowMus* window_;

  DISALLOW_COPY_AND_ASSIGN(WmWindowMusTestApi);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_WINDOW_MUS_TEST_API_H_
