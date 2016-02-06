// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PACKAGE_MANAGER_CONTENT_HANDLER_CONNECTION_H_
#define MOJO_SHELL_PACKAGE_MANAGER_CONTENT_HANDLER_CONNECTION_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/shell/identity.h"
#include "mojo/shell/public/interfaces/content_handler.mojom.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {
class ApplicationManager;

// A ContentHandlerConnection is responsible for creating and maintaining a
// connection to an app which provides the ContentHandler service.
// A ContentHandlerConnection can only be destroyed via CloseConnection.
// A ContentHandlerConnection manages its own lifetime and cannot be used with
// a scoped_ptr to avoid reentrant calls into ApplicationManager late in
// destruction.
class ContentHandlerConnection {
 public:
  using ClosedCallback = base::Callback<void(ContentHandlerConnection*)>;
  // |id| is a unique identifier for this content handler.
  ContentHandlerConnection(ApplicationManager* manager,
                           const Identity& source,
                           const Identity& content_handler,
                           uint32_t id,
                           const ClosedCallback& connection_closed_callback);

  void StartApplication(InterfaceRequest<mojom::Application> request,
                        URLResponsePtr response);

  // Closes the connection and destroys |this| object.
  void CloseConnection();

  const Identity& identity() const { return identity_; }
  uint32_t id() const { return id_; }

 private:
  ~ContentHandlerConnection();

  void ApplicationDestructed();

  ClosedCallback connection_closed_callback_;
  Identity identity_;

  mojom::ContentHandlerPtr content_handler_;
  bool connection_closed_;
  // The id for this content handler.
  const uint32_t id_;
  int ref_count_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerConnection);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_PACKAGE_MANAGER_CONTENT_HANDLER_CONNECTION_H_
