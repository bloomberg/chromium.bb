// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_TRANSIENT_WINDOW_MANAGER_H_
#define COMPONENTS_MUS_WS_TRANSIENT_WINDOW_MANAGER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/mus/ws/server_window_observer.h"

namespace mus {
namespace ws {

class TransientWindowObserver;

// TransientWindowManager manages the set of transient children for a window
// along with the transient parent. Transient children get the following
// behavior:
// . The transient parent destroys any transient children when it is
//   destroyed. This means a transient child is destroyed if either its parent
//   or transient parent is destroyed.
// . TODO(fsamuel): If a transient child and its transient parent share the same
//   parent, then transient children are always ordered above the transient
//   parent.
// Transient windows are typically used for popups and menus.
class TransientWindowManager : public ServerWindowObserver {
 public:
  using Windows = std::vector<ServerWindow*>;

  ~TransientWindowManager() override;

  // Returns the TransientWindowManager for |window|. This never returns
  // nullptr.
  static TransientWindowManager* GetOrCreate(ServerWindow* window);

  // Returns the TransientWindowManager for |window| only if it already exists.
  // WARNING: this may return nullptr.
  static TransientWindowManager* Get(ServerWindow* window);

  void AddObserver(TransientWindowObserver* observer);
  void RemoveObserver(TransientWindowObserver* observer);

  void AddTransientChild(ServerWindow* child);
  void RemoveTransientChild(ServerWindow* child);

  const Windows& transient_children() const { return transient_children_; }

  ServerWindow* transient_parent() { return transient_parent_; }
  const ServerWindow* transient_parent() const { return transient_parent_; }

 private:
  explicit TransientWindowManager(ServerWindow* window);

  // ServerWindowObserver:
  void OnWillDestroyWindow(ServerWindow* window) override;
  void OnWindowDestroyed(ServerWindow* window) override;

  ServerWindow* window_;
  ServerWindow* transient_parent_;
  Windows transient_children_;

  base::ObserverList<TransientWindowObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(TransientWindowManager);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_TRANSIENT_WINDOW_MANAGER_H_
