// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_

#include <string>
#include <vector>

#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/interfaces/input_events/input_events.mojom.h"

namespace mojo {
class ApplicationConnection;
namespace view_manager {

class Node;
class View;
class ViewManagerDelegate;
class WindowManagerDelegate;

class ViewManager {
 public:
  // Delegate is owned by the caller.
  static void ConfigureIncomingConnection(ApplicationConnection* connection,
                                          ViewManagerDelegate* delegate);

  // Sets the window manager delegate. Can only be called by the app embedded at
  // the service root node.
  virtual void SetWindowManagerDelegate(
      WindowManagerDelegate* window_manager_delegate) = 0;

  // Dispatches the supplied event to the specified View. Can be called only
  // by the application that called SetWindowManagerDelegate().
  virtual void DispatchEvent(View* target, EventPtr event) = 0;

  // Returns the URL of the application that embedded this application.
  virtual const std::string& GetEmbedderURL() const = 0;

  // Returns all root nodes known to this connection.
  virtual const std::vector<Node*>& GetRoots() const = 0;

  // Returns a Node or View known to this connection.
  virtual Node* GetNodeById(Id id) = 0;
  virtual View* GetViewById(Id id) = 0;

 protected:
  virtual ~ViewManager() {}

};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
