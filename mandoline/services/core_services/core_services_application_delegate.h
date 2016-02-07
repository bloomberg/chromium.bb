// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_SERVICES_CORE_SERVICES_CORE_SERVICES_APPLICATION_DELEGATE_H_
#define MANDOLINE_SERVICES_CORE_SERVICES_CORE_SERVICES_APPLICATION_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "components/clipboard/public/interfaces/clipboard.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/interface_factory_impl.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/content_handler.mojom.h"

namespace core_services {

class ApplicationThread;

// The CoreServices application is a singleton ServiceProvider. There is one
// instance of the CoreServices ServiceProvider.
class CoreServicesApplicationDelegate
    : public mojo::ShellClient,
      public mojo::InterfaceFactory<mojo::shell::mojom::ContentHandler>,
      public mojo::shell::mojom::ContentHandler {
 public:
  CoreServicesApplicationDelegate();
  ~CoreServicesApplicationDelegate() override;

  void ApplicationThreadDestroyed(ApplicationThread* thread);

 private:
  // Overridden from mojo::ShellClient:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;
  void Quit() override;

  // Overridden from mojo::InterfaceFactory<mojo::shell::mojom::ContentHandler>:
  void Create(
      mojo::Connection* connection,
      mojo::InterfaceRequest<mojo::shell::mojom::ContentHandler>
          request) override;

  // Overridden from mojo::shell::mojom::ContentHandler:
  void StartApplication(
      mojo::InterfaceRequest<mojo::shell::mojom::Application> request,
      mojo::URLResponsePtr response,
      const mojo::Callback<void()>& destruct_callback) override;

  // Bindings for all of our connections.
  mojo::WeakBindingSet<mojo::shell::mojom::ContentHandler> handler_bindings_;

  ScopedVector<ApplicationThread> application_threads_;
  mojo::TracingImpl tracing_;

  base::WeakPtrFactory<CoreServicesApplicationDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CoreServicesApplicationDelegate);
};

}  // namespace core_services

#endif  // MANDOLINE_SERVICES_CORE_SERVICES_CORE_SERVICES_APPLICATION_DELEGATE_H_
