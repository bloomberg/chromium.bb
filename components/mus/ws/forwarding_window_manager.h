// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_FORWARDING_WINDOW_MANAGER_H_
#define COMPONENTS_MUS_WS_FORWARDING_WINDOW_MANAGER_H_

#include "base/macros.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"

namespace mus {
namespace ws {

class ConnectionManager;

// WindowManager implementation that forwards to the window manager of the
// active display.
class ForwardingWindowManager : public mojom::WindowManager {
 public:
  explicit ForwardingWindowManager(ConnectionManager* connection_manager);
  ~ForwardingWindowManager() override;

 private:
  // Returns the WindowManager of the active display.
  mojom::WindowManager* GetActiveWindowManager();

  // mojom::WindowManager:
  void OpenWindow(
      mus::mojom::WindowTreeClientPtr client,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) override;
  void SetPreferredSize(uint32_t window_id,
                        mojo::SizePtr size,
                        const SetPreferredSizeCallback& callback) override;
  void SetResizeBehavior(uint32_t window_id,
                         mus::mojom::ResizeBehavior resize_behavior) override;
  void GetConfig(const GetConfigCallback& callback) override;

  ConnectionManager* connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingWindowManager);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_FORWARDING_WINDOW_MANAGER_H_
