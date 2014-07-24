// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_CLIENT_FACTORY_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_CLIENT_FACTORY_H_

#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

namespace mojo {

class ViewManagerDelegate;

// Add an instance of this class to an incoming connection to allow it to
// instantiate ViewManagerClient implementations in response to
// ViewManagerClient requests.
class ViewManagerClientFactory : public InterfaceFactory<ViewManagerClient> {
 public:
  explicit ViewManagerClientFactory(ViewManagerDelegate* delegate);
  virtual ~ViewManagerClientFactory();

  // InterfaceFactory<ViewManagerClient> implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<ViewManagerClient> request)
      MOJO_OVERRIDE;

 private:
  ViewManagerDelegate* delegate_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_CLIENT_FACTORY_H_
