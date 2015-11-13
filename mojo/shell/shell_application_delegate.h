// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SHELL_APPLICATION_DELEGATE_H_
#define MOJO_SHELL_SHELL_APPLICATION_DELEGATE_H_

#include "mojo/application/public/cpp/application_delegate.h"

#include "base/macros.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/shell/application_manager.mojom.h"

namespace mojo {
namespace shell {
class ApplicationManager;

class ShellApplicationDelegate
    : public ApplicationDelegate,
      public InterfaceFactory<mojom::ApplicationManager>,
      public mojom::ApplicationManager {
 public:
  explicit ShellApplicationDelegate(mojo::shell::ApplicationManager* manager);
  ~ShellApplicationDelegate() override;

 private:
  // Overridden from ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override;

  // Overridden from InterfaceFactory<mojom::ApplicationManager>:
  void Create(
      ApplicationConnection* connection,
      InterfaceRequest<mojom::ApplicationManager> request) override;

  // Overridden from mojom::ApplicationManager:
  void CreateInstanceForHandle(ScopedHandle channel,
                               const String& url,
                               const String& qualifier) override;

  mojo::shell::ApplicationManager* manager_;

  WeakBindingSet<mojom::ApplicationManager> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ShellApplicationDelegate);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SHELL_APPLICATION_DELEGATE_H_
