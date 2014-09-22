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
class View;
class ViewManagerDelegate;
class WindowManagerDelegate;

// Encapsulates a connection to the view manager service.
// A unique connection is made for every unique embed path for an app. e.g. for
// app B embed by the following paths: A->B, A->C->B - there are two connections
// and thus two instances of this class.
class ViewManager {
 public:
  // Sets the window manager delegate. Can only be called by the app embedded at
  // the service root view. Can only be called once.
  virtual void SetWindowManagerDelegate(
      WindowManagerDelegate* window_manager_delegate) = 0;

  // Dispatches the supplied event to the specified View. Can be called only
  // by the application that called SetWindowManagerDelegate().
  virtual void DispatchEvent(View* target, EventPtr event) = 0;

  // Returns the URL of the application that embedded this application.
  virtual const std::string& GetEmbedderURL() const = 0;

  // Returns all root views known to this connection.
  virtual const std::vector<View*>& GetRoots() const = 0;

  // Returns a View known to this connection.
  virtual View* GetViewById(Id id) = 0;

 protected:
  virtual ~ViewManager() {}
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
