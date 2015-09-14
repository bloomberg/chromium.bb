// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_VIEW_TREE_CONNECTION_H_
#define COMPONENTS_MUS_PUBLIC_CPP_VIEW_TREE_CONNECTION_H_

#include <string>

#include "components/mus/public/cpp/types.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace mojo {
class View;
class ViewTreeDelegate;

// Encapsulates a connection to a view tree. A unique connection is made
// every time an app is embedded.
class ViewTreeConnection {
 public:
  virtual ~ViewTreeConnection() {}

  // The returned ViewTreeConnection instance owns itself, and is deleted when
  // the last root is destroyed or the connection to the service is broken.
  static ViewTreeConnection* Create(ViewTreeDelegate* delegate,
                                    InterfaceRequest<ViewTreeClient> request);

  // Returns the root of this connection.
  virtual View* GetRoot() = 0;

  // Returns a View known to this connection.
  virtual View* GetViewById(Id id) = 0;

  // Returns the focused view; null if focus is not yet known or another app is
  // focused.
  virtual View* GetFocusedView() = 0;

  // Creates and returns a new View (which is owned by the ViewManager). Views
  // are initially hidden, use SetVisible(true) to show.
  virtual View* CreateView() = 0;

  // Returns true if ACCESS_POLICY_EMBED_ROOT was specified.
  virtual bool IsEmbedRoot() = 0;

  // Returns the id for this connection.
  virtual ConnectionSpecificId GetConnectionId() = 0;
};

}  // namespace mojo

#endif  // COMPONENTS_MUS_PUBLIC_CPP_VIEW_TREE_CONNECTION_H_
