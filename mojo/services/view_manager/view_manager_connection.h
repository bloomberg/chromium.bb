// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_

#include <set>

#include "base/basictypes.h"
#include "mojo/public/cpp/shell/service.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

namespace mojo {
namespace services {
namespace view_manager {

class RootViewManager;
class View;

// Manages a connection from the client.
class ViewManagerConnection : public Service<ViewManager, ViewManagerConnection,
                                             RootViewManager> {
 public:
  ViewManagerConnection();
  virtual ~ViewManagerConnection();

  int32_t id() const { return id_; }

  // Invoked from Service when connection is established.
  void Initialize(
      ServiceFactory<ViewManagerConnection, RootViewManager>* service_factory,
      ScopedMessagePipeHandle client_handle);

  // Returns the View by id.
  View* GetView(int32_t id);

 private:
  typedef std::map<int32_t, View*> ViewMap;

  // Returns the View by ViewId.
  View* GetViewById(const ViewId& view_id);

  // Overridden from ViewManager:
  virtual void CreateView(int32_t view_id,
                          const mojo::Callback<void(bool)>& callback) OVERRIDE;
  virtual void AddView(const ViewId& parent_id,
                       const ViewId& child_id,
                       const mojo::Callback<void(bool)>& callback) OVERRIDE;
  virtual void RemoveFromParent(
      const ViewId& view_id,
      const mojo::Callback<void(bool)>& callback) OVERRIDE;

  // Id of this connection as assigned by RootViewManager. Assigned in
  // Initialize().
  int32_t id_;

  ViewMap view_map_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerConnection);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_
