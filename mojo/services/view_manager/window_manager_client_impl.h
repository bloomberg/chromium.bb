// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_WINDOW_MANAGER_CLIENT_IMPL_H_
#define MOJO_SERVICES_VIEW_MANAGER_WINDOW_MANAGER_CLIENT_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "mojo/services/public/interfaces/window_manager/window_manager.mojom.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace service {

class ConnectionManager;

#if defined(OS_WIN)
// Equivalent of NON_EXPORTED_BASE which does not work with the template snafu
// below.
#pragma warning(push)
#pragma warning(disable : 4275)
#endif

class MOJO_VIEW_MANAGER_EXPORT WindowManagerClientImpl
    : public InterfaceImpl<WindowManagerClient> {
 public:
  explicit WindowManagerClientImpl(ConnectionManager* connection_manager);
  ~WindowManagerClientImpl() override;

  // WindowManagerClient:
  void DispatchInputEventToView(Id transport_view_id, EventPtr event) override;

  // InterfaceImp overrides:
  void OnConnectionError() override;

 private:
  ConnectionManager* connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerClientImpl);
};

#if defined(OS_WIN)
#pragma warning(pop)
#endif

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_WINDOW_MANAGER_CLIENT_IMPL_H_
