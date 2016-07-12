// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_
#define CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_

#include <memory>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/common/mojo_application_info.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/interfaces/service.mojom.h"

namespace shell {
class Connection;
class Connector;
class ServiceContext;
}

namespace content {

// Encapsulates a connection to a //services/shell.
// Access a global instance on the thread the ServiceContext was bound by
// calling Holder::Get().
// Clients can add shell::Service implementations whose exposed interfaces
// will be exposed to inbound connections to this object's Service.
// Alternatively clients can define named services that will be constructed when
// requests for those service names are received.
// Clients must call any of the registration methods when receiving
// ContentBrowserClient::RegisterInProcessMojoApplications().
class CONTENT_EXPORT MojoShellConnection {
 public:
  using ServiceRequestHandler =
      base::Callback<void(shell::mojom::ServiceRequest)>;
  using Factory = base::Callback<std::unique_ptr<MojoShellConnection>(void)>;

  // Stores an instance of |connection| in TLS for the current process. Must be
  // called on the thread the connection was created on.
  static void SetForProcess(std::unique_ptr<MojoShellConnection> connection);

  // Returns the per-process instance, or nullptr if the Shell connection has
  // not yet been bound. Must be called on the thread the connection was created
  // on.
  static MojoShellConnection* GetForProcess();

  // Destroys the per-process instance. Must be called on the thread the
  // connection was created on.
  static void DestroyForProcess();

  virtual ~MojoShellConnection();

  // Sets the factory used to create the MojoShellConnection. This must be
  // called before the MojoShellConnection has been created.
  static void SetFactoryForTest(Factory* factory);

  // Creates a MojoShellConnection from |request|.
  static std::unique_ptr<MojoShellConnection> Create(
      shell::mojom::ServiceRequest request);

  // Returns the bound shell::ServiceContext object.
  // TODO(rockot): remove.
  virtual shell::ServiceContext* GetShellConnection() = 0;

  // Returns the shell::Connector received via this connection's Service
  // implementation. Use this to initiate connections as this object's Identity.
  virtual shell::Connector* GetConnector() = 0;

  // Returns this connection's identity with the shell. Connections initiated
  // via the shell::Connector returned by GetConnector() will use this.
  virtual const shell::Identity& GetIdentity() const = 0;

  // Sets a closure that is called when the connection is lost. Note that
  // connection may already have been closed, in which case |closure| will be
  // run immediately before returning from this function.
  virtual void SetConnectionLostClosure(const base::Closure& closure) = 0;

  // Allows the caller to expose interfaces to the caller using the identity of
  // this object's Service. As distinct from MergeService() and
  // AddServiceRequestHandler() which specify unique identities for the
  // registered services.
  virtual void MergeService(std::unique_ptr<shell::Service> service) = 0;
  virtual void MergeService(shell::Service* service) = 0;

  // Adds an embedded service to this connection's ServiceFactory.
  // |info| provides details on how to construct new instances of the
  // service when an incoming connection is made to |name|.
  virtual void AddEmbeddedService(const std::string& name,
                                  const MojoApplicationInfo& info) = 0;

  // Adds a generic ServiceRequestHandler for a given service name. This
  // will be used to satisfy any incoming calls to CreateService() which
  // reference the given name.
  //
  // For in-process services, it is preferable to use |AddEmbeddedService()| as
  // defined above.
  virtual void AddServiceRequestHandler(
      const std::string& name,
      const ServiceRequestHandler& handler) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_
