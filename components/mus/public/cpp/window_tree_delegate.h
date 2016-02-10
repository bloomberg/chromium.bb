// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_DELEGATE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_DELEGATE_H_

#include <string>

#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/shell/public/interfaces/interface_provider.mojom.h"

namespace mus {

class Window;
class WindowTreeConnection;

// Interface implemented by an application using the window manager.
//
// WindowTreeConnection is deleted by any of the following:
// . If all the roots of the connection are destroyed and the connection is
//   configured to delete when there are no roots (the default). This happens
//   if the owner of the roots Embed()s another app in all the roots, or all
//   the roots are explicitly deleted.
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
  // NOTE: this is only invoked if the WindowTreeConnection is created with
  // an InterfaceRequest.
  virtual void OnEmbed(Window* root) = 0;

  // Sent when another app is embedded in |root| (one of the roots of the
  // connection). Afer this call |root| is deleted. If |root| is the only root
  // and the connection is configured to delete when there are no roots (the
  // default), then after |root| is destroyed the connection is destroyed as
  // well.
  virtual void OnUnembed(Window* root);

  // Called from the destructor of WindowTreeConnection after all the Windows
  // have been destroyed. |connection| is no longer valid after this call.
  virtual void OnConnectionLost(WindowTreeConnection* connection) = 0;

 protected:
  virtual ~WindowTreeDelegate() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_DELEGATE_H_
