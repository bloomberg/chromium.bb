// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_MANAGER_SERVICE_MANAGER_CONNECTION_IMPL_H_
#define CONTENT_COMMON_SERVICE_MANAGER_SERVICE_MANAGER_CONNECTION_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/interfaces/service.mojom.h"

namespace shell {
class Connector;
}

namespace content {

class EmbeddedServiceRunner;

class ServiceManagerConnectionImpl : public ServiceManagerConnection {
 public:
  explicit ServiceManagerConnectionImpl(
      shell::mojom::ServiceRequest request,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~ServiceManagerConnectionImpl() override;

 private:
  class IOThreadContext;

  // ServiceManagerConnection:
  void Start() override;
  void SetInitializeHandler(const base::Closure& handler) override;
  shell::Connector* GetConnector() override;
  const shell::Identity& GetIdentity() const override;
  void SetConnectionLostClosure(const base::Closure& closure) override;
  void SetupInterfaceRequestProxies(
      shell::InterfaceRegistry* registry,
      shell::InterfaceProvider* provider) override;
  int AddConnectionFilter(std::unique_ptr<ConnectionFilter> filter) override;
  void RemoveConnectionFilter(int filter_id) override;
  void AddEmbeddedService(const std::string& name,
                          const ServiceInfo& info) override;
  void AddServiceRequestHandler(
      const std::string& name,
      const ServiceRequestHandler& handler) override;

  void OnContextInitialized(const shell::Identity& identity);
  void OnConnectionLost();
  void CreateService(shell::mojom::ServiceRequest request,
                     const std::string& name);
  void GetInterface(shell::mojom::InterfaceProvider* provider,
                    const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle request_handle);

  shell::Identity identity_;

  std::unique_ptr<shell::Connector> connector_;
  scoped_refptr<IOThreadContext> context_;

  base::Closure initialize_handler_;
  base::Closure connection_lost_handler_;

  std::unordered_map<std::string, std::unique_ptr<EmbeddedServiceRunner>>
      embedded_services_;
  std::unordered_map<std::string, ServiceRequestHandler> request_handlers_;

  base::WeakPtrFactory<ServiceManagerConnectionImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerConnectionImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_MANAGER_SERVICE_MANAGER_CONNECTION_IMPL_H_
