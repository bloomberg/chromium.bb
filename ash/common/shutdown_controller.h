// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHUTDOWN_CONTROLLER_H_
#define ASH_COMMON_SHUTDOWN_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/shutdown.mojom.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace ash {

// Handles actual device shutdown by making requests to powerd over D-Bus.
// Caches the DeviceRebootOnShutdown device policy sent from Chrome over mojo.
class ASH_EXPORT ShutdownController
    : NON_EXPORTED_BASE(public mojom::ShutdownController) {
 public:
  ShutdownController();
  ~ShutdownController() override;

  bool reboot_on_shutdown() const { return reboot_on_shutdown_; }

  // Shuts down or reboots based on the current DeviceRebootOnShutdown policy.
  // Does not trigger the shutdown fade-out animation. For animated shutdown
  // use WmShell::RequestShutdown(). Virtual for testing.
  virtual void ShutDownOrReboot();

  // Binds the mojom::ShutdownController interface request to this object.
  void BindRequest(mojom::ShutdownControllerRequest request);

 private:
  // mojom::ShutdownController:
  void SetRebootOnShutdown(bool reboot_on_shutdown) override;

  // Cached copy of the DeviceRebootOnShutdown policy from chrome.
  bool reboot_on_shutdown_ = false;

  // Bindings for the ShutdownController interface.
  mojo::BindingSet<mojom::ShutdownController> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownController);
};

}  // namespace ash

#endif  // ASH_COMMON_SHUTDOWN_CONTROLLER_H_
