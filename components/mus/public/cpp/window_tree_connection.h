// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CONNECTION_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CONNECTION_H_

#include <string>

#include "components/mus/public/cpp/types.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace mus {

class Window;
class WindowTreeDelegate;

// Encapsulates a connection to a window tree. A unique connection is made
// every time an app is embedded.
class WindowTreeConnection {
 public:
  enum class CreateType {
    // Indicates Create() should wait for OnEmbed(). If true, the
    // WindowTreeConnection returned from Create() will have its root, otherwise
    // the WindowTreeConnection will get the root at a later time.
    WAIT_FOR_EMBED,
    DONT_WAIT_FOR_EMBED
  };

  virtual ~WindowTreeConnection() {}

  // The returned WindowTreeConnection instance owns itself, and is deleted when
  // the last root is destroyed or the connection to the service is broken.
  static WindowTreeConnection* Create(
      WindowTreeDelegate* delegate,
      mojo::InterfaceRequest<mojom::WindowTreeClient> request,
      CreateType create_type);

  // Returns the root of this connection.
  virtual Window* GetRoot() = 0;

  // Returns a Window known to this connection.
  virtual Window* GetWindowById(Id id) = 0;

  // Returns the focused window; null if focus is not yet known or another app
  // is focused.
  virtual Window* GetFocusedWindow() = 0;

  // Creates and returns a new Window (which is owned by the ViewManager). Views
  // are initially hidden, use SetVisible(true) to show.
  virtual Window* NewWindow() = 0;

  // Returns true if ACCESS_POLICY_EMBED_ROOT was specified.
  virtual bool IsEmbedRoot() = 0;

  // Returns the id for this connection.
  virtual ConnectionSpecificId GetConnectionId() = 0;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CONNECTION_H_
