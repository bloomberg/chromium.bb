// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_SHELL_CONNECTION_H_
#define MOJO_SHELL_PUBLIC_CPP_SHELL_CONNECTION_H_

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"

namespace mojo {

class AppRefCountImpl;

// Encapsulates a connection to the Mojo Shell in two parts:
// - a bound InterfacePtr to mojo::shell::mojom::Shell, the primary mechanism
//   by which the instantiating application interacts with other services
//   brokered by the Mojo Shell.
// - a bound InterfaceRequest of mojo::shell::mojom::ShellClient, an interface
//   used by the Mojo Shell to inform this application of lifecycle events and
//   inbound connections brokered by it.
//
// This class should be used in two scenarios:
// - During early startup to bind the mojo::shell::mojom::ShellClientRequest
//   obtained from the Mojo Shell. This is typically in response to either
//   MojoMain() or main().
// - In an implementation of mojo::shell::mojom::ContentHandler to bind the
//   mojo::shell::mojom::ShellClientRequest passed via StartApplication. In this
//   scenario there can be many instances of this class per process.
//
// Instances of this class are constructed with an implementation of the Shell
// Client Lib's mojo::ShellClient interface. See documentation in shell_client.h
// for details.
//
// Though this class provides the canonical implementation of the Shell Client
// lib's mojo::Shell interface, this interface should not be reached through
// pointers to this type.
class ShellConnection : public Shell, public shell::mojom::ShellClient {
 public:
  class TestApi {
   public:
    explicit TestApi(ShellConnection* shell_connection)
        : shell_connection_(shell_connection) {}

    void UnbindConnections(
        InterfaceRequest<shell::mojom::ShellClient>* request,
        shell::mojom::ShellPtr* shell) {
      shell_connection_->UnbindConnections(request, shell);
    }

   private:
    ShellConnection* shell_connection_;
  };

  // Does not take ownership of |delegate|, which must remain valid for the
  // lifetime of ShellConnection.
  ShellConnection(mojo::ShellClient* client,
                  InterfaceRequest<shell::mojom::ShellClient> request);
  // Constructs an ShellConnection with a custom termination closure. This
  // closure is invoked on Quit() instead of the default behavior of quitting
  // the current base::MessageLoop.
  ShellConnection(mojo::ShellClient* client,
                  InterfaceRequest<shell::mojom::ShellClient> request,
                  const Closure& termination_closure);
  ~ShellConnection() override;

  // Block the calling thread until the Initialize() method is called by the
  // shell.
  void WaitForInitialize();

 private:
  friend AppRefCountImpl;

  // Shell:
  scoped_ptr<Connection> Connect(const std::string& url) override;
  scoped_ptr<Connection> Connect(ConnectParams* params) override;
  void Quit() override;
  scoped_ptr<AppRefCount> CreateAppRefCount() override;

  // shell::mojom::ShellClient:
  void Initialize(shell::mojom::ShellPtr shell,
                  const mojo::String& url,
                  uint32_t id) override;
  void AcceptConnection(const String& requestor_url,
                        uint32_t requestor_id,
                        InterfaceRequest<InterfaceProvider> remote_interfaces,
                        InterfaceProviderPtr local_interfaces,
                        Array<String> allowed_interfaces,
                        const String& url) override;
  void OnQuitRequested(const Callback<void(bool)>& callback) override;

  void OnConnectionError();

  // Called from Quit() when there is no Shell connection, or asynchronously
  // from Quit() once the Shell has OK'ed shutdown.
  void QuitNow();

  // Unbinds the Shell and Application connections. Can be used to re-bind the
  // handles to another implementation of ShellConnection, for instance when
  // running apptests.
  void UnbindConnections(InterfaceRequest<shell::mojom::ShellClient>* request,
                         shell::mojom::ShellPtr* shell);

  // Called from AppRefCountImpl.
  void AddRef();
  void Release();

  // We track the lifetime of incoming connection registries as it more
  // convenient for the client.
  ScopedVector<Connection> incoming_connections_;
  mojo::ShellClient* client_;
  Binding<shell::mojom::ShellClient> binding_;
  shell::mojom::ShellPtr shell_;
  Closure termination_closure_;
  bool quit_requested_;
  int ref_count_;
  base::WeakPtrFactory<ShellConnection> weak_factory_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ShellConnection);
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_SHELL_CONNECTION_H_
