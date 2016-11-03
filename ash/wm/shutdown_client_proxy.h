// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SHUTDOWN_CLIENT_PROXY_H_
#define ASH_WM_SHUTDOWN_CLIENT_PROXY_H_

#include "ash/public/interfaces/shutdown.mojom.h"
#include "base/macros.h"

namespace service_manager {
class Connector;
}

namespace ash {

// A ShutdownClientProxy which lazily connects to an exported
// mojom::ShutdownClient in "service:content_browser" on each use. This exists
// so we can replace an instance of this class with a TestShutdownClient in
// tests.
class ShutdownClientProxy : public mojom::ShutdownClient {
 public:
  explicit ShutdownClientProxy(service_manager::Connector* connector);
  ~ShutdownClientProxy() override;

  // mojom::ShutdownClient:
  void RequestShutdown() override;

 private:
  service_manager::Connector* connector_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownClientProxy);
};

}  // namespace ash

#endif  // ASH_WM_SHUTDOWN_CLIENT_PROXY_H_
