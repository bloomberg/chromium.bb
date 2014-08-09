// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_VIEW_MANAGER_LOADER_H_
#define MOJO_SHELL_VIEW_MANAGER_LOADER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "mojo/application_manager/application_loader.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/view_manager_init_service_context.h"

namespace mojo {

class Application;

namespace shell {

// ApplicationLoader responsible for creating connections to the ViewManager.
class ViewManagerLoader : public ApplicationLoader,
                          public ApplicationDelegate,
                          public InterfaceFactory<ViewManagerInitService> {
 public:
  ViewManagerLoader();
  virtual ~ViewManagerLoader();

 private:
  // ApplicationLoader overrides:
  virtual void Load(ApplicationManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE;
  virtual void OnServiceError(ApplicationManager* manager,
                              const GURL& url) OVERRIDE;

  // ApplicationDelegate overrides.
  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) OVERRIDE;

  // InterfaceFactory<ViewManagerInitService> overrides.
  virtual void Create(
      ApplicationConnection* connection,
      InterfaceRequest<ViewManagerInitService> request) OVERRIDE;

  ScopedVector<Application> apps_;
  service::ViewManagerInitServiceContext context_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_VIEW_MANAGER_LOADER_H_
