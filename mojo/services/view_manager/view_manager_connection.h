// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_

#include <set>

#include "base/basictypes.h"
#include "mojo/public/cpp/shell/service.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/view_delegate.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace services {
namespace view_manager {

class RootViewManager;
class View;

// Manages a connection from the client.
class MOJO_VIEW_MANAGER_EXPORT ViewManagerConnection
    : public ServiceConnection<ViewManager, ViewManagerConnection,
                               RootViewManager>,
      public ViewDelegate {
 public:
  ViewManagerConnection();
  virtual ~ViewManagerConnection();

  int32_t id() const { return id_; }

  // Invoked from Service when connection is established.
  void Initialize(
      ServiceConnector<ViewManagerConnection, RootViewManager>* service_factory,
      ScopedMessagePipeHandle client_handle);

  // Returns the View by id.
  View* GetView(int32_t id);

  // Notifies the client of a hierarchy change.
  void NotifyViewHierarchyChanged(const ViewId& view,
                                  const ViewId& new_parent,
                                  const ViewId& old_parent,
                                  int32_t change_id);

 private:
  typedef std::map<int32_t, View*> ViewMap;

  // Returns the View by ViewId.
  View* GetViewById(const ViewId& view_id);

  // Overridden from ViewManager:
  virtual void CreateView(int32_t view_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void AddView(const ViewId& parent_id,
                       const ViewId& child_id,
                       int32_t change_id,
                       const Callback<void(bool)>& callback) OVERRIDE;
  virtual void RemoveViewFromParent(
      const ViewId& view,
      int32_t change_id,
      const Callback<void(bool)>& callback) OVERRIDE;

  // Overriden from ViewDelegate:
  virtual ViewId GetViewId(const View* view) const OVERRIDE;
  virtual void OnViewHierarchyChanged(const ViewId& view,
                                      const ViewId& new_parent,
                                      const ViewId& old_parent) OVERRIDE;

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
