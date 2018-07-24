// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WS_WINDOW_SERVICE_OWNER_H_
#define ASH_WS_WINDOW_SERVICE_OWNER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shell_init_params.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {
class ServiceContext;
}

namespace ui {
namespace ws2 {
class GpuInterfaceProvider;
class WindowService;
}  // namespace ws2
}  // namespace ui

namespace ash {

class WindowServiceDelegateImpl;

// WindowServiceOwner indirectly owns the WindowService. This class is
// responsible for responding to the ServiceRequest for the WindowService. When
// BindWindowService() is called the WindowService is created.
class ASH_EXPORT WindowServiceOwner {
 public:
  explicit WindowServiceOwner(
      std::unique_ptr<ui::ws2::GpuInterfaceProvider> gpu_interface_provider);
  ~WindowServiceOwner();

  using RegisterInterfacesCallback =
      base::OnceCallback<void(service_manager::BinderRegistry*)>;
  // Sets a callback used to register test only interfaces on the WindowService.
  // The callback is run once the WindowService is created (this must be called
  // before the WindowService is created).
  static void SetRegisterWindowServiceInterfacesCallback(
      RegisterInterfacesCallback cb);

  // Called from the ServiceManager when a request is made for the
  // WindowService.
  void BindWindowService(service_manager::mojom::ServiceRequest request);

  // Returns the WindowService, or null if BindWindowService() hasn't been
  // called yet.
  ui::ws2::WindowService* window_service() { return window_service_; }

 private:
  friend class AshTestHelper;

  // Non-null until |service_context_| is created.
  std::unique_ptr<ui::ws2::GpuInterfaceProvider> gpu_interface_provider_;

  // The following state is created once BindWindowService() is called.

  std::unique_ptr<WindowServiceDelegateImpl> window_service_delegate_;

  // Handles the ServiceRequest. Owns |window_service_|.
  std::unique_ptr<service_manager::ServiceContext> service_context_;

  // The WindowService. This is owned by |service_context_|.
  ui::ws2::WindowService* window_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WindowServiceOwner);
};

}  // namespace ash

#endif  // ASH_WS_WINDOW_SERVICE_OWNER_H_
