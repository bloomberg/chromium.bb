// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CONNECTION_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CONNECTION_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "components/mus/common/types.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace mojo {
class ApplicationImpl;
}

namespace mus {

class Window;
class WindowManagerDelegate;
class WindowTreeConnectionObserver;
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

  // Creates a WindowTreeConnection with no roots. Use this to establish a
  // connection directly to mus and create top level windows.
  static WindowTreeConnection* Create(WindowTreeDelegate* delegate,
                                      mojo::ApplicationImpl* app);

  // Creates a WindowTreeConnection to service the specified request for
  // a WindowTreeClient. Use this to be embedded in another app.
  static WindowTreeConnection* Create(
      WindowTreeDelegate* delegate,
      mojo::InterfaceRequest<mojom::WindowTreeClient> request,
      CreateType create_type);

  // Create a WindowTreeConnection that is going to serve as the WindowManager.
  static WindowTreeConnection* CreateForWindowManager(
      WindowTreeDelegate* delegate,
      mojo::InterfaceRequest<mojom::WindowTreeClient> request,
      CreateType create_type,
      WindowManagerDelegate* window_manager_delegate);

  // Sets whether this is deleted when there are no roots. The default is to
  // delete when there are no roots.
  virtual void SetDeleteOnNoRoots(bool value) = 0;

  // Returns the root of this connection.
  virtual const std::set<Window*>& GetRoots() = 0;

  // Returns a Window known to this connection.
  virtual Window* GetWindowById(Id id) = 0;

  // Returns the focused window; null if focus is not yet known or another app
  // is focused.
  virtual Window* GetFocusedWindow() = 0;

  // Creates and returns a new Window (which is owned by the window server).
  // Windows are initially hidden, use SetVisible(true) to show.
  Window* NewWindow() { return NewWindow(nullptr); }
  virtual Window* NewWindow(
      const std::map<std::string, std::vector<uint8_t>>* properties) = 0;
  virtual Window* NewTopLevelWindow(
      const std::map<std::string, std::vector<uint8_t>>* properties) = 0;

  // Returns true if ACCESS_POLICY_EMBED_ROOT was specified.
  virtual bool IsEmbedRoot() = 0;

  // Returns the id for this connection.
  virtual ConnectionSpecificId GetConnectionId() = 0;

  virtual void AddObserver(WindowTreeConnectionObserver* observer) = 0;
  virtual void RemoveObserver(WindowTreeConnectionObserver* observer) = 0;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CONNECTION_H_
