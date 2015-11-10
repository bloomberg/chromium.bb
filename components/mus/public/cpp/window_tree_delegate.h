// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_DELEGATE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_DELEGATE_H_

#include <string>

#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

namespace mus {

class Window;
class WindowTreeConnection;

// Interface implemented by an application using the window manager.
//
// Each call to OnEmbed() results in a new WindowTreeConnection and new root
// Window.
// WindowTreeConnection is deleted by any of the following:
// . If the root of the connection is destroyed. This happens if the owner
//   of the root Embed()s another app in root, or root is explicitly deleted.
// . The connection to the window manager is lost.
// . Explicitly by way of calling delete.
//
// When the WindowTreeConnection is deleted all windows are deleted (and
// observers notified). This is followed by notifying the delegate by way of
// OnConnectionLost().
class WindowTreeDelegate {
 public:
  // Called when the application implementing this interface is embedded at
  // |root|.
  virtual void OnEmbed(Window* root) = 0;

  // Sent when another app is embedded in the same Window as this connection.
  // Subsequently the root Window and this object are destroyed (observers are
  // notified appropriately).
  virtual void OnUnembed();

  // Called from the destructor of WindowTreeConnection after all the Windows
  // have been destroyed. |connection| is no longer valid after this call.
  virtual void OnConnectionLost(WindowTreeConnection* connection) = 0;

 protected:
  virtual ~WindowTreeDelegate() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_DELEGATE_H_
