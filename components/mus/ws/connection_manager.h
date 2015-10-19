// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_CONNECTION_MANAGER_H_
#define COMPONENTS_MUS_WS_CONNECTION_MANAGER_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/focus_controller_delegate.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/server_window_delegate.h"
#include "components/mus/ws/server_window_observer.h"
#include "components/mus/ws/window_tree_host_impl.h"
#include "mojo/converters/surfaces/custom_surface_converter.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mus {

namespace ws {

class ClientConnection;
class ConnectionManagerDelegate;
class ServerWindow;
class WindowTreeHostConnection;
class WindowTreeImpl;

// ConnectionManager manages the set of connections to the window server (all
// the WindowTreeImpls) as well as providing the root of the hierarchy.
class ConnectionManager : public ServerWindowDelegate,
                          public ServerWindowObserver {
 public:
  // Create when a WindowTreeImpl is about to make a change. Ensures clients are
  // notified correctly.
  class ScopedChange {
   public:
    ScopedChange(WindowTreeImpl* connection,
                 ConnectionManager* connection_manager,
                 bool is_delete_window);
    ~ScopedChange();

    ConnectionSpecificId connection_id() const { return connection_id_; }
    bool is_delete_window() const { return is_delete_window_; }

    // Marks the connection with the specified id as having seen a message.
    void MarkConnectionAsMessaged(ConnectionSpecificId connection_id) {
      message_ids_.insert(connection_id);
    }

    // Returns true if MarkConnectionAsMessaged(connection_id) was invoked.
    bool DidMessageConnection(ConnectionSpecificId connection_id) const {
      return message_ids_.count(connection_id) > 0;
    }

   private:
    ConnectionManager* connection_manager_;
    const ConnectionSpecificId connection_id_;
    const bool is_delete_window_;

    // See description of MarkConnectionAsMessaged/DidMessageConnection.
    std::set<ConnectionSpecificId> message_ids_;

    DISALLOW_COPY_AND_ASSIGN(ScopedChange);
  };

  ConnectionManager(ConnectionManagerDelegate* delegate,
                    const scoped_refptr<mus::SurfacesState>& surfaces_state);
  ~ConnectionManager() override;

  // Adds a WindowTreeHost.
  void AddHost(WindowTreeHostConnection* connection);

  // Creates a new ServerWindow. The return value is owned by the caller, but
  // must be destroyed before ConnectionManager.
  ServerWindow* CreateServerWindow(const WindowId& id);

  // Returns the id for the next WindowTreeImpl.
  ConnectionSpecificId GetAndAdvanceNextConnectionId();

  // Returns the id for the next WindowTreeHostImpl.
  uint16_t GetAndAdvanceNextHostId();

  // Invoked when a WindowTreeImpl's connection encounters an error.
  void OnConnectionError(ClientConnection* connection);

  // Invoked when a WindowTreeHostConnection encounters an error or the
  // associated Display window is closed.
  void OnHostConnectionClosed(WindowTreeHostConnection* connection);

  // See description of WindowTree::Embed() for details. This assumes
  // |transport_window_id| is valid.
  void EmbedAtWindow(ConnectionSpecificId creator_id,
                     const WindowId& window_id,
                     uint32_t policy_bitmask,
                     mojo::URLRequestPtr request);
  WindowTreeImpl* EmbedAtWindow(ConnectionSpecificId creator_id,
                                const WindowId& window_id,
                                uint32_t policy_bitmask,
                                mojom::WindowTreeClientPtr client);

  // Returns the connection by id.
  WindowTreeImpl* GetConnection(ConnectionSpecificId connection_id);

  // Returns the Window identified by |id|.
  ServerWindow* GetWindow(const WindowId& id);

  // Returns whether |window| is a descendant of some root window but not itself
  // a root window.
  bool IsWindowAttachedToRoot(const ServerWindow* window) const;

  // Schedules a paint for the specified region in the coordinates of |window|.
  void SchedulePaint(const ServerWindow* window, const gfx::Rect& bounds);

  bool IsProcessingChange() const { return current_change_ != nullptr; }

  bool is_processing_delete_window() const {
    return current_change_ && current_change_->is_delete_window();
  }

  // Invoked when the WindowTreeHostImpl's display is closed.
  void OnDisplayClosed();

  // Invoked when a connection messages a client about the change. This is used
  // to avoid sending ServerChangeIdAdvanced() unnecessarily.
  void OnConnectionMessagedClient(ConnectionSpecificId id);

  // Returns true if OnConnectionMessagedClient() was invoked for id.
  bool DidConnectionMessageClient(ConnectionSpecificId id) const;

  // Returns the metrics of the viewport where the provided |window| is
  // displayed.
  mojom::ViewportMetricsPtr GetViewportMetricsForWindow(
      const ServerWindow* window);

  // Returns the WindowTreeImpl that has |id| as a root.
  WindowTreeImpl* GetConnectionWithRoot(const WindowId& id) {
    return const_cast<WindowTreeImpl*>(
        const_cast<const ConnectionManager*>(this)->GetConnectionWithRoot(id));
  }
  const WindowTreeImpl* GetConnectionWithRoot(const WindowId& id) const;

  WindowTreeHostImpl* GetWindowTreeHostByWindow(const ServerWindow* window);
  const WindowTreeHostImpl* GetWindowTreeHostByWindow(
      const ServerWindow* window) const;

  // Returns the first ancestor of |service| that is marked as an embed root.
  WindowTreeImpl* GetEmbedRoot(WindowTreeImpl* service);

  // WindowTreeHost implementation helper; see mojom for details.
  bool CloneAndAnimate(const WindowId& window_id);

  // These functions trivially delegate to all WindowTreeImpls, which in
  // term notify their clients.
  void ProcessWindowDestroyed(ServerWindow* window);
  void ProcessWindowBoundsChanged(const ServerWindow* window,
                                  const gfx::Rect& old_bounds,
                                  const gfx::Rect& new_bounds);
  void ProcessClientAreaChanged(const ServerWindow* window,
                                const gfx::Rect& old_client_area,
                                const gfx::Rect& new_client_area);
  void ProcessViewportMetricsChanged(const mojom::ViewportMetrics& old_metrics,
                                     const mojom::ViewportMetrics& new_metrics);
  void ProcessWillChangeWindowHierarchy(const ServerWindow* window,
                                        const ServerWindow* new_parent,
                                        const ServerWindow* old_parent);
  void ProcessWindowHierarchyChanged(const ServerWindow* window,
                                     const ServerWindow* new_parent,
                                     const ServerWindow* old_parent);
  void ProcessWindowReorder(const ServerWindow* window,
                            const ServerWindow* relative_window,
                            const mojom::OrderDirection direction);
  void ProcessWindowDeleted(const WindowId& window);

 private:
  using ConnectionMap = std::map<ConnectionSpecificId, ClientConnection*>;
  using HostConnectionMap =
      std::map<WindowTreeHostImpl*, WindowTreeHostConnection*>;

  // Invoked when a connection is about to make a change. Subsequently followed
  // by FinishChange() once the change is done.
  //
  // Changes should never nest, meaning each PrepareForChange() must be
  // balanced with a call to FinishChange() with no PrepareForChange()
  // in between.
  void PrepareForChange(ScopedChange* change);

  // Balances a call to PrepareForChange().
  void FinishChange();

  // Returns true if the specified connection originated the current change.
  bool IsChangeSource(ConnectionSpecificId connection_id) const {
    return current_change_ && current_change_->connection_id() == connection_id;
  }

  // Adds |connection| to internal maps.
  void AddConnection(ClientConnection* connection);

  // Overridden from ServerWindowDelegate:
  mus::SurfacesState* GetSurfacesState() override;
  void OnScheduleWindowPaint(const ServerWindow* window) override;
  const ServerWindow* GetRootWindow(const ServerWindow* window) const override;

  // Overridden from ServerWindowObserver:
  void OnWindowDestroyed(ServerWindow* window) override;
  void OnWillChangeWindowHierarchy(ServerWindow* window,
                                   ServerWindow* new_parent,
                                   ServerWindow* old_parent) override;
  void OnWindowHierarchyChanged(ServerWindow* window,
                                ServerWindow* new_parent,
                                ServerWindow* old_parent) override;
  void OnWindowBoundsChanged(ServerWindow* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowClientAreaChanged(ServerWindow* window,
                                 const gfx::Rect& old_client_area,
                                 const gfx::Rect& new_client_area) override;
  void OnWindowReordered(ServerWindow* window,
                         ServerWindow* relative,
                         mojom::OrderDirection direction) override;
  void OnWillChangeWindowVisibility(ServerWindow* window) override;
  void OnWindowSharedPropertyChanged(
      ServerWindow* window,
      const std::string& name,
      const std::vector<uint8_t>* new_data) override;
  void OnWindowTextInputStateChanged(ServerWindow* window,
                                     const ui::TextInputState& state) override;

  ConnectionManagerDelegate* delegate_;

  // State for rendering into a Surface.
  scoped_refptr<mus::SurfacesState> surfaces_state_;

  // ID to use for next WindowTreeImpl.
  ConnectionSpecificId next_connection_id_;

  // ID to use for next WindowTreeHostImpl.
  uint16_t next_host_id_;

  // Set of WindowTreeImpls.
  ConnectionMap connection_map_;

  // Set of WindowTreeHostImpls.
  HostConnectionMap host_connection_map_;

  // If non-null we're processing a change. The ScopedChange is not owned by us
  // (it's created on the stack by WindowTreeImpl).
  ScopedChange* current_change_;

  bool in_destructor_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
};

}  // namespace ws

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_CONNECTION_MANAGER_H_
