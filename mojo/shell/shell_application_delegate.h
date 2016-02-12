// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SHELL_APPLICATION_DELEGATE_H_
#define MOJO_SHELL_SHELL_APPLICATION_DELEGATE_H_

#include <stdint.h>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/application_manager.mojom.h"

namespace mojo {
namespace shell {
class ApplicationManager;

class ShellApplicationDelegate
    : public ShellClient,
      public InterfaceFactory<mojom::ApplicationManager>,
      public mojom::ApplicationManager {
 public:
  explicit ShellApplicationDelegate(mojo::shell::ApplicationManager* manager);
  ~ShellApplicationDelegate() override;

 private:
  // Overridden from ShellClient:
  void Initialize(Shell* shell, const std::string& url, uint32_t id) override;
  bool AcceptConnection(Connection* connection) override;

  // Overridden from InterfaceFactory<mojom::ApplicationManager>:
  void Create(
      Connection* connection,
      InterfaceRequest<mojom::ApplicationManager> request) override;

  // Overridden from mojom::ApplicationManager:
  void CreateInstanceForHandle(
      ScopedHandle channel,
      const String& url,
      mojom::CapabilityFilterPtr filter,
      InterfaceRequest<mojom::PIDReceiver> pid_receiver) override;
  void AddListener(
      mojom::ApplicationManagerListenerPtr listener) override;

  mojo::shell::ApplicationManager* manager_;

  WeakBindingSet<mojom::ApplicationManager> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ShellApplicationDelegate);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SHELL_APPLICATION_DELEGATE_H_
