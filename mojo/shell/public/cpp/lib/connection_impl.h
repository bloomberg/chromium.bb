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
#include "mojo/shell/public/interfaces/interface_provider.mojom.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"

namespace mojo {
namespace internal {

// A ConnectionImpl represents each half of a connection between two
// applications, allowing customization of which interfaces are published to the
// other.
class ConnectionImpl : public Connection {
 public:
  ConnectionImpl();
  // |allowed_interfaces| are the set of interfaces that the shell has allowed
  // an application to expose to another application. If this set contains only
  // the string value "*" all interfaces may be exposed.
  ConnectionImpl(const std::string& connection_url,
                 const std::string& remote_url,
                 uint32_t remote_id,
                 shell::mojom::InterfaceProviderPtr remote_interfaces,
                 shell::mojom::InterfaceProviderRequest local_interfaces,
                 const std::set<std::string>& allowed_interfaces);
  ~ConnectionImpl() override;

  shell::mojom::Shell::ConnectToApplicationCallback
      GetConnectToApplicationCallback();

 private:
  // Connection:
  const std::string& GetConnectionURL() override;
  const std::string& GetRemoteApplicationURL() override;
  void SetRemoteInterfaceProviderConnectionErrorHandler(
      const Closure& handler) override;
  bool GetRemoteApplicationID(uint32_t* remote_id) const override;
  bool GetRemoteContentHandlerID(uint32_t* content_handler_id) const override;
  void AddRemoteIDCallback(const Closure& callback) override;
  bool AllowsInterface(const std::string& interface_name) const override;
  shell::mojom::InterfaceProvider* GetRemoteInterfaces() override;
  InterfaceRegistry* GetLocalRegistry() override;
  base::WeakPtr<Connection> GetWeakPtr() override;

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

  InterfaceRegistry local_registry_;
  shell::mojom::InterfaceProviderPtr remote_interfaces_;

  const std::set<std::string> allowed_interfaces_;
  const bool allow_all_interfaces_;

  base::WeakPtrFactory<ConnectionImpl> weak_factory_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ConnectionImpl);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_
