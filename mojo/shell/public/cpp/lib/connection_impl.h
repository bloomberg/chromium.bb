// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_
#define MOJO_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_

#include <stdint.h>

#include <set>
#include <string>

#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/interfaces/service_provider.mojom.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"

namespace mojo {
namespace internal {

// A ConnectionImpl represents each half of a connection between two
// applications, allowing customization of which services are published to the
// other.
class ConnectionImpl : public Connection, public ServiceProvider {
 public:
  class TestApi {
  public:
    explicit TestApi(ConnectionImpl* impl) : impl_(impl) {}
    ~TestApi() {}

    void SetInterfaceBinderForName(InterfaceBinder* binder,
                                   const std::string& interface_name) {
      impl_->SetInterfaceBinderForName(binder, interface_name);
    }
    void RemoveInterfaceBinderForName(const std::string& interface_name) {
      impl_->RemoveInterfaceBinderForName(interface_name);
    }

  private:
    ConnectionImpl* impl_;
    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  ConnectionImpl();
  // |allowed_interfaces| are the set of interfaces that the shell has allowed
  // an application to expose to another application. If this set contains only
  // the string value "*" all interfaces may be exposed.
  ConnectionImpl(const std::string& connection_url,
                 const std::string& remote_url,
                 uint32_t remote_id,
                 ServiceProviderPtr remote_services,
                 InterfaceRequest<ServiceProvider> local_services,
                 const std::set<std::string>& allowed_interfaces);
  ~ConnectionImpl() override;

  shell::mojom::Shell::ConnectToApplicationCallback
      GetConnectToApplicationCallback();

 private:
  using NameToInterfaceBinderMap = std::map<std::string, InterfaceBinder*>;

  // Connection overrides.
  void SetDefaultInterfaceBinder(InterfaceBinder* binder) override;
  bool SetInterfaceBinderForName(InterfaceBinder* binder,
                                 const std::string& interface_name) override;
  const std::string& GetConnectionURL() override;
  const std::string& GetRemoteApplicationURL() override;
  ServiceProvider* GetServiceProvider() override;
  ServiceProvider* GetLocalServiceProvider() override;
  void SetRemoteServiceProviderConnectionErrorHandler(
      const Closure& handler) override;
  bool GetRemoteApplicationID(uint32_t* remote_id) const override;
  bool GetRemoteContentHandlerID(uint32_t* content_handler_id) const override;
  void AddRemoteIDCallback(const Closure& callback) override;
  base::WeakPtr<Connection> GetWeakPtr() override;

  // ServiceProvider method.
  void ConnectToService(const mojo::String& service_name,
                        ScopedMessagePipeHandle handle) override;

  void RemoveInterfaceBinderForName(const std::string& interface_name);
  void OnGotRemoteIDs(uint32_t target_application_id,
                      uint32_t content_handler_id);

  const std::string connection_url_;
  const std::string remote_url_;

  uint32_t remote_id_;
  // The id of the content_handler is only available once the callback from
  // establishing the connection is made.
  uint32_t content_handler_id_;
  bool remote_ids_valid_;
  std::vector<Closure> remote_id_callbacks_;

  Binding<ServiceProvider> local_binding_;
  ServiceProviderPtr remote_service_provider_;

  const std::set<std::string> allowed_interfaces_;
  const bool allow_all_interfaces_;

  InterfaceBinder* default_binder_;
  NameToInterfaceBinderMap name_to_binder_;

  base::WeakPtrFactory<ConnectionImpl> weak_factory_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ConnectionImpl);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_
