// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_
#define CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "content/public/common/mojo_shell_connection.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/public/interfaces/service_factory.mojom.h"

namespace content {

class EmbeddedApplicationRunner;

class MojoShellConnectionImpl
    : public MojoShellConnection,
      public shell::Service,
      public shell::InterfaceFactory<shell::mojom::ServiceFactory>,
      public shell::mojom::ServiceFactory {

 public:
  explicit MojoShellConnectionImpl(shell::mojom::ServiceRequest request);
  ~MojoShellConnectionImpl() override;

 private:
  // MojoShellConnection:
  shell::ServiceContext* GetShellConnection() override;
  shell::Connector* GetConnector() override;
  const shell::Identity& GetIdentity() const override;
  void SetConnectionLostClosure(const base::Closure& closure) override;
  void MergeService(std::unique_ptr<shell::Service> service) override;
  void MergeService(shell::Service* service) override;
  void AddEmbeddedService(const std::string& name,
                          const MojoApplicationInfo& info) override;
  void AddServiceRequestHandler(
      const std::string& name,
      const ServiceRequestHandler& handler) override;

  // shell::Service:
  void OnStart(shell::Connector* connector,
               const shell::Identity& identity,
               uint32_t id) override;
  bool OnConnect(shell::Connection* connection) override;
  shell::InterfaceRegistry* GetInterfaceRegistryForConnection() override;
  shell::InterfaceProvider* GetInterfaceProviderForConnection() override;

  // shell::InterfaceFactory<shell::mojom::ServiceFactory>:
  void Create(shell::Connection* connection,
              shell::mojom::ServiceFactoryRequest request) override;

  // shell::mojom::ServiceFactory:
  void CreateService(shell::mojom::ServiceRequest request,
                         const mojo::String& name) override;

  std::unique_ptr<shell::ServiceContext> shell_connection_;
  mojo::BindingSet<shell::mojom::ServiceFactory> factory_bindings_;
  std::vector<shell::Service*> embedded_services_;
  std::vector<std::unique_ptr<shell::Service>> owned_services_;
  std::unordered_map<std::string, std::unique_ptr<EmbeddedApplicationRunner>>
      embedded_apps_;
  std::unordered_map<std::string, ServiceRequestHandler> request_handlers_;

  DISALLOW_COPY_AND_ASSIGN(MojoShellConnectionImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_
