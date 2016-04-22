// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_DISCONNECTED_APP_HANDLER_H_
#define MASH_WM_DISCONNECTED_APP_HANDLER_H_

#include "base/macros.h"
#include "components/mus/public/cpp/window_tracker.h"

namespace mash {
namespace wm {

// Tracks mus::Windows for when they get disconnected from the embedded app.
// Currently closes a window upon disconnection.
class DisconnectedAppHandler : public mus::WindowTracker {
 public:
  DisconnectedAppHandler();
  ~DisconnectedAppHandler() override;

 private:
  // mus::WindowObserver:
  void OnWindowEmbeddedAppDisconnected(mus::Window* window) override;

  DISALLOW_COPY_AND_ASSIGN(DisconnectedAppHandler);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_DISCONNECTED_APP_HANDLER_H_
