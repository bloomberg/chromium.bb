// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_IMPL_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {

class ApplicationConnection;
class ServiceProvider;

namespace service {

class ViewManagerInitServiceContext;

#if defined(OS_WIN)
// Equivalent of NON_EXPORTED_BASE which does not work with the template snafu
// below.
#pragma warning(push)
#pragma warning(disable : 4275)
#endif

// Used to create the initial ViewManagerClient. Doesn't initiate the Connect()
// until the WindowTreeHost has been created.
class MOJO_VIEW_MANAGER_EXPORT ViewManagerInitServiceImpl
    : public InterfaceImpl<ViewManagerInitService> {
 public:
  ViewManagerInitServiceImpl(ApplicationConnection* connection,
                             ViewManagerInitServiceContext* context);
  virtual ~ViewManagerInitServiceImpl();

  void OnNativeViewportDeleted();

  void OnRootViewManagerWindowTreeHostCreated();

 private:
  struct ConnectParams {
    ConnectParams();
    ~ConnectParams();

    std::string url;
    Callback<void(bool)> callback;
  };

  void MaybeEmbed();

  // ViewManagerInitService overrides:
  virtual void Embed(const String& url,
                     const Callback<void(bool)>& callback) OVERRIDE;

  ViewManagerInitServiceContext* context_;

  ServiceProvider* service_provider_;

  // Stores information about inbound calls to Embed() pending execution on
  // the window tree host being ready to use.
  ScopedVector<ConnectParams> connect_params_;

  bool is_tree_host_ready_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerInitServiceImpl);
};

#if defined(OS_WIN)
#pragma warning(pop)
#endif

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_IMPL_H_
