// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SHELL_CLIENT_FACTORY_CONNECTION_H_
#define MOJO_SHELL_SHELL_CLIENT_FACTORY_CONNECTION_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/shell/identity.h"
#include "mojo/shell/public/interfaces/shell_client_factory.mojom.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {
class ApplicationManager;

// A ShellClientFactoryConnection is responsible for creating and maintaining a
// connection to an app which provides the ShellClientFactory service.
// A ShellClientFactoryConnection can only be destroyed via CloseConnection.
// A ShellClientFactoryConnection manages its own lifetime and cannot be used
// with a scoped_ptr to avoid reentrant calls into ApplicationManager late in
// destruction.
class ShellClientFactoryConnection {
 public:
  using ClosedCallback = base::Callback<void(ShellClientFactoryConnection*)>;
  // |id| is a unique identifier for this content handler.
  ShellClientFactoryConnection(ApplicationManager* manager,
                           const Identity& source,
                           const Identity& shell_client_factory,
                           uint32_t id,
                           const ClosedCallback& connection_closed_callback);

  void CreateShellClient(mojom::ShellClientRequest request, const GURL& url);

  // Closes the connection and destroys |this| object.
  void CloseConnection();

  const Identity& identity() const { return identity_; }
  uint32_t id() const { return id_; }

 private:
  ~ShellClientFactoryConnection();

  void ApplicationDestructed();

  ClosedCallback connection_closed_callback_;
  Identity identity_;

  mojom::ShellClientFactoryPtr shell_client_factory_;
  bool connection_closed_;
  // The id for this content handler.
  const uint32_t id_;
  int ref_count_;

  DISALLOW_COPY_AND_ASSIGN(ShellClientFactoryConnection);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SHELL_CLIENT_FACTORY_CONNECTION_H_
