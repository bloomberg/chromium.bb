// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_CONNECTION_MANAGER_H_
#define COMPONENTS_MUS_WS_CONNECTION_MANAGER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "components/mus/public/interfaces/display.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/operation.h"
#include "components/mus/ws/server_window_delegate.h"
#include "components/mus/ws/server_window_observer.h"
#include "components/mus/ws/window_tree_host_impl.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/common/weak_interface_ptr_set.h"
#include "mojo/converters/surfaces/custom_surface_converter.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {
namespace ws {

class ClientConnection;
class ConnectionManagerDelegate;
class ServerWindow;
class WindowTreeHostConnection;
class WindowTreeImpl;

// ConnectionManager manages the set of connections to the window server (all
// the WindowTreeImpls) as well as providing the root of the hierarchy.
//
// TODO(sky): this class is doing too much. Refactor to make responsibilities
// clearer.
class ConnectionManager : public ServerWindowDelegate,
                          public ServerWindowObserver,
                          public mojom::DisplayManager {
 public:
  ConnectionManager(ConnectionManagerDelegate* delegate,
                    const scoped_refptr<mus::SurfacesState>& surfaces_state);
  ~ConnectionManager() override;

  // Adds a WindowTreeHost.
  void AddHost(WindowTreeHostConnection* connection);

  // Creates a new ServerWindow. The return value is owned by the caller, but
  // must be destroyed before ConnectionManager.
  ServerWindow* CreateServerWindow(
      const WindowId& id,
      const std::map<std::string, std::vector<uint8_t>>& properties);

  // Returns the id for the next WindowTreeImpl.
  ConnectionSpecificId GetAndAdvanceNextConnectionId();

  // Returns the id for the next WindowTreeHostImpl.
  uint16_t GetAndAdvanceNextHostId();

  // Invoked when a WindowTreeImpl's connection encounters an error.
  void OnConnectionError(ClientConnection* connection);

  ClientConnection* GetClientConnection(WindowTreeImpl* window_tree);

  // Invoked when a WindowTreeHostConnection encounters an error or the
  // associated Display window is closed.
  void OnHostConnectionClosed(WindowTreeHostConnection* connection);

  // See description of WindowTree::Embed() for details. This assumes
  // |transport_window_id| is valid.
  WindowTreeImpl* EmbedAtWindow(ServerWindow* root,
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

  OperationType current_operation_type() const {
    return current_operation_ ? current_operation_->type()
                              : OperationType::NONE;
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
  WindowTreeImpl* GetConnectionWithRoot(const ServerWindow* window) {
    return const_cast<WindowTreeImpl*>(
        const_cast<const ConnectionManager*>(this)
            ->GetConnectionWithRoot(window));
  }
  const WindowTreeImpl* GetConnectionWithRoot(const ServerWindow* window) const;

  WindowTreeHostImpl* GetWindowTreeHostByWindow(const ServerWindow* window);
  const WindowTreeHostImpl* GetWindowTreeHostByWindow(
      const ServerWindow* window) const;

  WindowTreeHostImpl* GetActiveWindowTreeHost();

  bool has_tree_host_connections() const {
    return !host_connection_map_.empty();
  }

  void AddDisplayManagerBinding(
      mojo::InterfaceRequest<mojom::DisplayManager> request);

  // Returns a change id for the window manager that is associated with
  // |source| and |client_change_id|. When the window manager replies
  // WindowManagerChangeCompleted() is called to obtain the original source
  // and client supplied change_id that initiated the called.
  uint32_t GenerateWindowManagerChangeId(WindowTreeImpl* source,
                                         uint32_t client_change_id);

  // Called when a response from the window manager is obtained. Calls to
  // the client that initiated the change with the change id originally
  // supplied by the client.
  void WindowManagerChangeCompleted(uint32_t window_manager_change_id,
                                    bool success);
  void WindowManagerCreatedTopLevelWindow(WindowTreeImpl* wm_connection,
                                          uint32_t window_manager_change_id,
                                          const ServerWindow* window);

  // Called when we get an unexpected message from the WindowManager.
  // TODO(sky): decide what we want to do here.
  void WindowManagerSentBogusMessage() {}

  // These functions trivially delegate to all WindowTreeImpls, which in
  // term notify their clients.
  void ProcessWindowDestroyed(ServerWindow* window);
  void ProcessWindowBoundsChanged(const ServerWindow* window,
                                  const gfx::Rect& old_bounds,
                                  const gfx::Rect& new_bounds);
  void ProcessClientAreaChanged(
      const ServerWindow* window,
      const gfx::Insets& new_client_area,
      const std::vector<gfx::Rect>& new_additional_client_areas);
  void ProcessViewportMetricsChanged(WindowTreeHostImpl* host,
                                     const mojom::ViewportMetrics& old_metrics,
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
  void ProcessWindowDeleted(const ServerWindow* window);
  void ProcessWillChangeWindowPredefinedCursor(ServerWindow* window,
                                               int32_t cursor_id);
  void ProcessFrameDecorationValuesChanged(WindowTreeHostImpl* host);

 private:
  friend class Operation;

  using ConnectionMap = std::map<ConnectionSpecificId, ClientConnection*>;
  using HostConnectionMap =
      std::map<WindowTreeHostImpl*, WindowTreeHostConnection*>;

  struct InFlightWindowManagerChange {
    // Identifies the client that initiated the change.
    ConnectionSpecificId connection_id;

    // Change id supplied by the client.
    uint32_t client_change_id;
  };

  using InFlightWindowManagerChangeMap =
      std::map<uint32_t, InFlightWindowManagerChange>;

  bool GetAndClearInFlightWindowManagerChange(
      uint32_t window_manager_change_id,
      InFlightWindowManagerChange* change);

  // Invoked when a connection is about to execute a window server operation.
  // Subsequently followed by FinishOperation() once the change is done.
  //
  // Changes should never nest, meaning each PrepareForOperation() must be
  // balanced with a call to FinishOperation() with no PrepareForOperation()
  // in between.
  void PrepareForOperation(Operation* op);

  // Balances a call to PrepareForOperation().
  void FinishOperation();

  // Returns true if the specified connection issued the current operation.
  bool IsOperationSource(ConnectionSpecificId connection_id) const {
    return current_operation_ &&
           current_operation_->source_connection_id() == connection_id;
  }

  // Adds |connection| to internal maps.
  void AddConnection(ClientConnection* connection);

  // Run in response to events which may cause us to change the native cursor.
  void MaybeUpdateNativeCursor(ServerWindow* window);

  // Calls OnDisplays() on |observer|.
  void CallOnDisplays(mojom::DisplayManagerObserver* observer);

  // Calls observer->OnDisplaysChanged() with the display for |host|.
  void CallOnDisplayChanged(mojom::DisplayManagerObserver* observer,
                            WindowTreeHostImpl* host);

  // Returns the Display for |host|.
  mojom::DisplayPtr DisplayForHost(WindowTreeHostImpl* host);

  // Overridden from ServerWindowDelegate:
  mus::SurfacesState* GetSurfacesState() override;
  void OnScheduleWindowPaint(const ServerWindow* window) override;
  const ServerWindow* GetRootWindow(const ServerWindow* window) const override;
  void ScheduleSurfaceDestruction(ServerWindow* window) override;

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
  void OnWindowClientAreaChanged(
      ServerWindow* window,
      const gfx::Insets& new_client_area,
      const std::vector<gfx::Rect>& new_additional_client_areas) override;
  void OnWindowReordered(ServerWindow* window,
                         ServerWindow* relative,
                         mojom::OrderDirection direction) override;
  void OnWillChangeWindowVisibility(ServerWindow* window) override;
  void OnWindowSharedPropertyChanged(
      ServerWindow* window,
      const std::string& name,
      const std::vector<uint8_t>* new_data) override;
  void OnWindowPredefinedCursorChanged(ServerWindow* window,
                                       int32_t cursor_id) override;
  void OnWindowTextInputStateChanged(ServerWindow* window,
                                     const ui::TextInputState& state) override;
  void OnTransientWindowAdded(ServerWindow* window,
                              ServerWindow* transient_child) override;
  void OnTransientWindowRemoved(ServerWindow* window,
                                ServerWindow* transient_child) override;

  // Overriden from mojom::DisplayManager:
  void AddObserver(mojom::DisplayManagerObserverPtr observer) override;

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

  // If non-null then we're processing a client operation. The Operation is
  // not owned by us (it's created on the stack by WindowTreeImpl).
  Operation* current_operation_;

  bool in_destructor_;

  // Maps from window manager change id to the client that initiated the
  // request.
  InFlightWindowManagerChangeMap in_flight_wm_change_map_;

  // Next id supplied to the window manager.
  uint32_t next_wm_change_id_;

  mojo::WeakBindingSet<mojom::DisplayManager> display_manager_bindings_;
  // WARNING: only use these once |got_valid_frame_decorations_| is true.
  // TODO(sky): refactor this out into its own class.
  mojo::WeakInterfacePtrSet<mojom::DisplayManagerObserver>
      display_manager_observers_;

  bool got_valid_frame_decorations_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_CONNECTION_MANAGER_H_
