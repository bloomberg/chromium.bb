// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_DELEGATE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"

namespace mojo {

class ServiceProviderImpl;
class View;
class ViewManager;

// Interface implemented by an application using the view manager.
class ViewManagerDelegate {
 public:
  // Called when the application implementing this interface is embedded at
  // |root|. |view_manager| is an implementation of an object bound to a
  // connection to the view manager service. The object is valid until
  // OnViewManagerDisconnected() is called with the same object.
  // This function is called for every embed, but there will be a unique
  // instance of |view_manager| only for every unique connection. See
  // the class description of ViewManager for some rules about when a new
  // connection is made.
  // |exported_services| is an object that the delegate can add services to to
  // expose to the embedder. |imported_services| exposes the services offered by
  // the embedder to the delegate. Note that if a different application is
  // subsequently embedded at |root|, the pipe(s) connecting |imported_services|
  // to the embedder and any services obtained from it are not broken and will
  // continue to be valid.
  virtual void OnEmbed(ViewManager* view_manager,
                       View* root,
                       ServiceProviderImpl* exported_services,
                       scoped_ptr<ServiceProvider> imported_services) = 0;

  // Called when a connection to the view manager service is closed.
  // |view_manager| is not valid after this function returns.
  virtual void OnViewManagerDisconnected(ViewManager* view_manager) = 0;

 protected:
  virtual ~ViewManagerDelegate() {}
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_DELEGATE_H_
