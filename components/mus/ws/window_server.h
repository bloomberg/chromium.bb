// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_SERVER_H_
#define COMPONENTS_MUS_WS_WINDOW_SERVER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/mus/public/interfaces/window_manager_factory.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/display.h"
#include "components/mus/ws/display_manager_delegate.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/operation.h"
#include "components/mus/ws/server_window_delegate.h"
#include "components/mus/ws/server_window_observer.h"
#include "components/mus/ws/user_id_tracker.h"
#include "components/mus/ws/window_manager_factory_registry.h"
#include "mojo/converters/surfaces/custom_surface_converter.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {
namespace ws {

class AccessPolicy;
class DisplayManager;
class ServerWindow;
class WindowManagerState;
class WindowServerDelegate;
class WindowTree;
class WindowTreeBinding;

// WindowServer manages the set of connections to the window server (all the
//  WindowTrees) as well as providing the root of the hierarchy.
class WindowServer : public ServerWindowDelegate,
                     public ServerWindowObserver,
                     public DisplayManagerDelegate {
 public:
  WindowServer(WindowServerDelegate* delegate,
               const scoped_refptr<mus::SurfacesState>& surfaces_state);
  ~WindowServer() override;

  WindowServerDelegate* delegate() { return delegate_; }

  UserIdTracker* user_id_tracker() { return &user_id_tracker_; }
  const UserIdTracker* user_id_tracker() const { return &user_id_tracker_; }

  DisplayManager* display_manager() { return display_manager_.get(); }
  const DisplayManager* display_manager() const {
    return display_manager_.get();
  }

  // Creates a new ServerWindow. The return value is owned by the caller, but
  // must be destroyed before WindowServer.
  ServerWindow* CreateServerWindow(
      const WindowId& id,
      const std::map<std::string, std::vector<uint8_t>>& properties);

  // Returns the id for the next WindowTree.
  ConnectionSpecificId GetAndAdvanceNextConnectionId();

  // See description of WindowTree::Embed() for details. This assumes
  // |transport_window_id| is valid.
  WindowTree* EmbedAtWindow(ServerWindow* root,
                            const UserId& user_id,
                            mojom::WindowTreeClientPtr client,
                            std::unique_ptr<AccessPolicy> access_policy);

  // Adds |tree_impl_ptr| to the set of known trees. Use DestroyTree() to
  // destroy the tree.
  WindowTree* AddTree(std::unique_ptr<WindowTree> tree_impl_ptr,
                      std::unique_ptr<WindowTreeBinding> binding,
                      mojom::WindowTreePtr tree_ptr);
  WindowTree* CreateTreeForWindowManager(Display* display,
                                         mojom::WindowManagerFactory* factory,
                                         ServerWindow* root,
                                         const UserId& user_id);
  // Invoked when a WindowTree's connection encounters an error.
  void DestroyTree(WindowTree* tree);

  // Returns the connection by id.
  WindowTree* GetTreeWithId(ConnectionSpecificId connection_id);

  WindowTree* GetTreeWithConnectionName(const std::string& connection_name);

  size_t num_trees() const { return tree_map_.size(); }

  // Returns the Window identified by |id|.
  ServerWindow* GetWindow(const WindowId& id);

  // Schedules a paint for the specified region in the coordinates of |window|.
  void SchedulePaint(ServerWindow* window, const gfx::Rect& bounds);

  OperationType current_operation_type() const {
    return current_operation_ ? current_operation_->type()
                              : OperationType::NONE;
  }

  // Returns true if the specified connection issued the current operation.
  bool IsOperationSource(ConnectionSpecificId tree_id) const {
    return current_operation_ &&
           current_operation_->source_tree_id() == tree_id;
  }

  // Invoked when a connection messages a client about the change. This is used
  // to avoid sending ServerChangeIdAdvanced() unnecessarily.
  void OnTreeMessagedClient(ConnectionSpecificId id);

  // Returns true if OnTreeMessagedClient() was invoked for id.
  bool DidTreeMessageClient(ConnectionSpecificId id) const;

  // Returns the metrics of the viewport where the provided |window| is
  // displayed.
  mojom::ViewportMetricsPtr GetViewportMetricsForWindow(
      const ServerWindow* window);

  // Returns the WindowTree that has |id| as a root.
  WindowTree* GetTreeWithRoot(const ServerWindow* window) {
    return const_cast<WindowTree*>(
        const_cast<const WindowServer*>(this)->GetTreeWithRoot(window));
  }
  const WindowTree* GetTreeWithRoot(const ServerWindow* window) const;

  void OnFirstWindowManagerFactorySet();

  WindowManagerFactoryRegistry* window_manager_factory_registry() {
    return &window_manager_factory_registry_;
  }

  // Sets focus to |window|. Returns true if |window| already has focus, or
  // focus was successfully changed. Returns |false| if |window| is not a valid
  // window to receive focus.
  bool SetFocusedWindow(ServerWindow* window);
  ServerWindow* GetFocusedWindow();

  // Returns a change id for the window manager that is associated with
  // |source| and |client_change_id|. When the window manager replies
  // WindowManagerChangeCompleted() is called to obtain the original source
  // and client supplied change_id that initiated the called.
  uint32_t GenerateWindowManagerChangeId(WindowTree* source,
                                         uint32_t client_change_id);

  // Called when a response from the window manager is obtained. Calls to
  // the client that initiated the change with the change id originally
  // supplied by the client.
  void WindowManagerChangeCompleted(uint32_t window_manager_change_id,
                                    bool success);
  void WindowManagerCreatedTopLevelWindow(WindowTree* wm_tree,
                                          uint32_t window_manager_change_id,
                                          const ServerWindow* window);

  // Called when we get an unexpected message from the WindowManager.
  // TODO(sky): decide what we want to do here.
  void WindowManagerSentBogusMessage() {}

  // These functions trivially delegate to all WindowTrees, which in
  // term notify their clients.
  void ProcessWindowBoundsChanged(const ServerWindow* window,
                                  const gfx::Rect& old_bounds,
                                  const gfx::Rect& new_bounds);
  void ProcessClientAreaChanged(
      const ServerWindow* window,
      const gfx::Insets& new_client_area,
      const std::vector<gfx::Rect>& new_additional_client_areas);
  void ProcessLostCapture(const ServerWindow* window);
  void ProcessViewportMetricsChanged(Display* display,
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

  // Sends an |event| to all WindowTrees belonging to |user_id| that might be
  // observing events. Skips |ignore_tree| if it is non-null.
  void SendToEventObservers(const ui::Event& event,
                            const UserId& user_id,
                            WindowTree* ignore_tree);

  // Sets a callback to be called whenever a ServerWindow is scheduled for
  // a [re]paint. This should only be called in a test configuration.
  void SetPaintCallback(const base::Callback<void(ServerWindow*)>& callback);

 private:
  friend class Operation;

  using WindowTreeMap =
      std::map<ConnectionSpecificId, std::unique_ptr<WindowTree>>;

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

  // Run in response to events which may cause us to change the native cursor.
  void MaybeUpdateNativeCursor(ServerWindow* window);

  // Overridden from ServerWindowDelegate:
  mus::SurfacesState* GetSurfacesState() override;
  void OnScheduleWindowPaint(ServerWindow* window) override;
  const ServerWindow* GetRootWindow(const ServerWindow* window) const override;
  void ScheduleSurfaceDestruction(ServerWindow* window) override;
  ServerWindow* FindWindowForSurface(
      const ServerWindow* ancestor,
      mojom::SurfaceType surface_type,
      const ClientWindowId& client_window_id) override;

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
  void OnWindowVisibilityChanged(ServerWindow* window) override;
  void OnWindowOpacityChanged(ServerWindow* window,
                              float old_opacity,
                              float new_opacity) override;
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

  // DisplayManagerDelegate:
  void OnFirstDisplayReady() override;
  void OnNoMoreDisplays() override;

  UserIdTracker user_id_tracker_;

  WindowServerDelegate* delegate_;

  // State for rendering into a Surface.
  scoped_refptr<mus::SurfacesState> surfaces_state_;

  // ID to use for next WindowTree.
  ConnectionSpecificId next_connection_id_;

  std::unique_ptr<DisplayManager> display_manager_;

  // Set of WindowTrees.
  WindowTreeMap tree_map_;

  // If non-null then we're processing a client operation. The Operation is
  // not owned by us (it's created on the stack by WindowTree).
  Operation* current_operation_;

  bool in_destructor_;

  // Maps from window manager change id to the client that initiated the
  // request.
  InFlightWindowManagerChangeMap in_flight_wm_change_map_;

  // Next id supplied to the window manager.
  uint32_t next_wm_change_id_;

  base::Callback<void(ServerWindow*)> window_paint_callback_;

  WindowManagerFactoryRegistry window_manager_factory_registry_;

  DISALLOW_COPY_AND_ASSIGN(WindowServer);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_SERVER_H_
