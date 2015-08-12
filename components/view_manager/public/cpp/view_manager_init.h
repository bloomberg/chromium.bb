// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_MANAGER_INIT_H_
#define COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_MANAGER_INIT_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mojo {

class ApplicationConnection;
class ApplicationImpl;
class ViewManagerDelegate;

// ViewManagerInit is used to establish the initial connection to the
// ViewManager. Use it when you know the ViewManager is not running and you're
// app is going to be the first one to contact it.
class ViewManagerInit {
 public:
  // |root_client| is optional.
  ViewManagerInit(ApplicationImpl* app,
                  ViewManagerDelegate* delegate,
                  ViewManagerRootClient* root_client);
  ~ViewManagerInit();

  // Returns the ViewManagerRoot. This is only valid if |root_client| was
  // supplied to the constructor.
  ViewManagerRoot* view_manager_root() { return view_manager_root_.get(); }

  // Returns the application connection established with the view manager.
  ApplicationConnection* connection() { return connection_.get(); }

 private:
  class ClientFactory;

  void OnCreate(InterfaceRequest<ViewManagerClient> request);

  ApplicationImpl* app_;
  scoped_ptr<ApplicationConnection> connection_;
  ViewManagerDelegate* delegate_;
  scoped_ptr<ClientFactory> client_factory_;
  ViewManagerRootPtr view_manager_root_;
  scoped_ptr<Binding<ViewManagerRootClient>> root_client_binding_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerInit);
};

}  // namespace mojo

#endif  // COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_MANAGER_INIT_H_
