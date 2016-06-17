// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_DISCONNECTED_APP_HANDLER_H_
#define ASH_MUS_DISCONNECTED_APP_HANDLER_H_

#include "base/macros.h"
#include "components/mus/public/cpp/window_tracker.h"

namespace ash {
namespace mus {

// Tracks mus::Windows for when they get disconnected from the embedded app.
// Destroys the window upon disconnection.
class DisconnectedAppHandler : public ::mus::WindowTracker {
 public:
  explicit DisconnectedAppHandler(::mus::Window* root_window);
  ~DisconnectedAppHandler() override;

 private:
  // mus::WindowObserver:
  void OnWindowEmbeddedAppDisconnected(::mus::Window* window) override;
  void OnTreeChanging(const TreeChangeParams& params) override;

  DISALLOW_COPY_AND_ASSIGN(DisconnectedAppHandler);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_DISCONNECTED_APP_HANDLER_H_
