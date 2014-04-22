// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_ROOT_VIEW_MANAGER_H_
#define MOJO_SERVICES_VIEW_MANAGER_ROOT_VIEW_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"
#include "mojo/services/view_manager/view.h"

namespace mojo {
namespace services {
namespace view_manager {

class View;
class ViewId;
class ViewManagerConnection;

// RootViewManager is responsible for managing the set of ViewManagerConnections
// as well as providing the root of the View hierarchy.
class RootViewManager : public NativeViewportClient {
 public:
  RootViewManager();
  virtual ~RootViewManager();

  // Returns the id for the next view manager.
  int32_t GetAndAdvanceNextConnectionId();

  void AddConnection(ViewManagerConnection* connection);
  void RemoveConnection(ViewManagerConnection* connection);

  // Returns the View identified by the specified pair, or NULL if there isn't
  // one.
  View* GetView(int32_t manager_id, int32_t view_id);

 private:
  typedef std::map<int32_t, ViewManagerConnection*> ConnectionMap;

  // Overridden from NativeViewportClient:
  virtual void OnCreated() OVERRIDE;
  virtual void OnDestroyed() OVERRIDE;
  virtual void OnBoundsChanged(const Rect& bounds) OVERRIDE;
  virtual void OnEvent(const Event& event,
                       const mojo::Callback<void()>& callback) OVERRIDE;

  // ID to use for next ViewManagerConnection.
  int32_t next_connection_id_;

  // Set of ViewManagerConnections.
  ConnectionMap connection_map_;

  // Root of Views that are displayed.
  View root_;

  DISALLOW_COPY_AND_ASSIGN(RootViewManager);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_ROOT_VIEW_MANAGER_H_
