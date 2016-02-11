// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_TREE_CLIENT_IMPL_H_
#define COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_TREE_CLIENT_IMPL_H_

#include <stdint.h>

#include <map>
#include <set>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/mus/common/types.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace gfx {
class Insets;
class Size;
}

namespace mus {
class InFlightChange;
class WindowTreeClientImplPrivate;
class WindowTreeConnection;
class WindowTreeDelegate;

enum class ChangeType;

// Manages the connection with the Window Server service.
class WindowTreeClientImpl : public WindowTreeConnection,
                             public mojom::WindowTreeClient,
                             public mojom::WindowManager,
                             public WindowManagerClient {
 public:
  WindowTreeClientImpl(WindowTreeDelegate* delegate,
                       WindowManagerDelegate* window_manager_delegate,
                       mojo::InterfaceRequest<mojom::WindowTreeClient> request);
  ~WindowTreeClientImpl() override;

  // Establishes the connection by way of the WindowTreeFactory.
  void ConnectViaWindowTreeFactory(mojo::Shell* shell);

  // Wait for OnEmbed(), returning when done.
  void WaitForEmbed();

  bool connected() const { return tree_ != nullptr; }
  ConnectionSpecificId connection_id() const { return connection_id_; }

  // API exposed to the window implementations that pushes local changes to the
  // service.
  void DestroyWindow(Window* window);

  // These methods take TransportIds. For windows owned by the current
  // connection, the connection id high word can be zero. In all cases, the
  // TransportId 0x1 refers to the root window.
  void AddChild(Window* parent, Id child_id);
  void RemoveChild(Window* parent, Id child_id);

  void AddTransientWindow(Window* window, Id transient_window_id);
  void RemoveTransientWindowFromParent(Window* window);

  void Reorder(Window* window,
               Id relative_window_id,
               mojom::OrderDirection direction);

  // Returns true if the specified window was created by this connection.
  bool OwnsWindow(Window* window) const;

  void SetBounds(Window* window,
                 const gfx::Rect& old_bounds,
                 const gfx::Rect& bounds);
  void SetCapture(Window* window);
  void ReleaseCapture(Window* window);
  void SetClientArea(Id window_id,
                     const gfx::Insets& client_area,
                     const std::vector<gfx::Rect>& additional_client_areas);
  void SetFocus(Window* window);
  void SetCanFocus(Id window_id, bool can_focus);
  void SetPredefinedCursor(Id window_id, mus::mojom::Cursor cursor_id);
  void SetVisible(Window* window, bool visible);
  void SetProperty(Window* window,
                   const std::string& name,
                   mojo::Array<uint8_t> data);
  void SetWindowTextInputState(Id window_id, mojo::TextInputStatePtr state);
  void SetImeVisibility(Id window_id,
                        bool visible,
                        mojo::TextInputStatePtr state);

  void Embed(Id window_id,
             mojom::WindowTreeClientPtr client,
             uint32_t policy_bitmask,
             const mojom::WindowTree::EmbedCallback& callback);

  void RequestClose(Window* window);

  void AttachSurface(Id window_id,
                     mojom::SurfaceType type,
                     mojo::InterfaceRequest<mojom::Surface> surface,
                     mojom::SurfaceClientPtr client);

  // Sets focus to |window| without notifying the server.
  void LocalSetFocus(Window* window);

  // Start/stop tracking windows. While tracked, they can be retrieved via
  // WindowTreeConnection::GetWindowById.
  void AddWindow(Window* window);

  bool IsRoot(Window* window) const { return roots_.count(window) > 0; }

  bool is_embed_root() const { return is_embed_root_; }

  // Called after the window's observers have been notified of destruction (as
  // the last step of ~Window).
  void OnWindowDestroyed(Window* window);

 private:
  friend class WindowTreeClientImplPrivate;

  enum class NewWindowType {
    CHILD,
    TOP_LEVEL,
  };

  using IdToWindowMap = std::map<Id, Window*>;

  // TODO(sky): this assumes change_ids never wrap, which is a bad assumption.
  using InFlightMap = std::map<uint32_t, scoped_ptr<InFlightChange>>;

  // Returns the oldest InFlightChange that matches |change|.
  InFlightChange* GetOldestInFlightChangeMatching(const InFlightChange& change);

  // See InFlightChange for details on how InFlightChanges are used.
  uint32_t ScheduleInFlightChange(scoped_ptr<InFlightChange> change);

  // Returns true if there is an InFlightChange that matches |change|. If there
  // is an existing change SetRevertValueFrom() is invoked on it. Returns false
  // if there is no InFlightChange matching |change|.
  // See InFlightChange for details on how InFlightChanges are used.
  bool ApplyServerChangeToExistingInFlightChange(const InFlightChange& change);

  Window* NewWindowImpl(NewWindowType type,
                        const Window::SharedProperties* properties);

  // OnEmbed() calls into this. Exposed as a separate function for testing.
  void OnEmbedImpl(mojom::WindowTree* window_tree,
                   ConnectionSpecificId connection_id,
                   mojom::WindowDataPtr root_data,
                   Id focused_window_id,
                   uint32_t access_policy);

  // Overridden from WindowTreeConnection:
  void SetDeleteOnNoRoots(bool value) override;
  const std::set<Window*>& GetRoots() override;
  Window* GetWindowById(Id id) override;
  Window* GetFocusedWindow() override;
  Window* NewWindow(const Window::SharedProperties* properties) override;
  Window* NewTopLevelWindow(
      const Window::SharedProperties* properties) override;
  bool IsEmbedRoot() override;
  ConnectionSpecificId GetConnectionId() override;
  void AddObserver(WindowTreeConnectionObserver* observer) override;
  void RemoveObserver(WindowTreeConnectionObserver* observer) override;

  // Overridden from WindowTreeClient:
  void OnEmbed(ConnectionSpecificId connection_id,
               mojom::WindowDataPtr root,
               mojom::WindowTreePtr tree,
               Id focused_window_id,
               uint32_t access_policy) override;
  void OnEmbeddedAppDisconnected(Id window_id) override;
  void OnUnembed(Id window_id) override;
  void OnLostCapture(Id window_id) override;
  void OnTopLevelCreated(uint32_t change_id,
                         mojom::WindowDataPtr data) override;
  void OnWindowBoundsChanged(Id window_id,
                             mojo::RectPtr old_bounds,
                             mojo::RectPtr new_bounds) override;
  void OnClientAreaChanged(
      uint32_t window_id,
      mojo::InsetsPtr new_client_area,
      mojo::Array<mojo::RectPtr> new_additional_client_areas) override;
  void OnTransientWindowAdded(uint32_t window_id,
                              uint32_t transient_window_id) override;
  void OnTransientWindowRemoved(uint32_t window_id,
                                uint32_t transient_window_id) override;
  void OnWindowViewportMetricsChanged(
      mojo::Array<uint32_t> window_ids,
      mojom::ViewportMetricsPtr old_metrics,
      mojom::ViewportMetricsPtr new_metrics) override;
  void OnWindowHierarchyChanged(
      Id window_id,
      Id new_parent_id,
      Id old_parent_id,
      mojo::Array<mojom::WindowDataPtr> windows) override;
  void OnWindowReordered(Id window_id,
                         Id relative_window_id,
                         mojom::OrderDirection direction) override;
  void OnWindowDeleted(Id window_id) override;
  void OnWindowVisibilityChanged(Id window_id, bool visible) override;
  void OnWindowDrawnStateChanged(Id window_id, bool drawn) override;
  void OnWindowSharedPropertyChanged(Id window_id,
                                     const mojo::String& name,
                                     mojo::Array<uint8_t> new_data) override;
  void OnWindowInputEvent(uint32_t event_id,
                          Id window_id,
                          mojom::EventPtr event) override;
  void OnWindowFocused(Id focused_window_id) override;
  void OnWindowPredefinedCursorChanged(Id window_id,
                                       mojom::Cursor cursor) override;
  void OnChangeCompleted(uint32_t change_id, bool success) override;
  void RequestClose(uint32_t window_id) override;
  void GetWindowManager(
      mojo::AssociatedInterfaceRequest<WindowManager> internal) override;

  // Overridden from WindowManager:
  void WmSetBounds(uint32_t change_id,
                   Id window_id,
                   mojo::RectPtr transit_bounds) override;
  void WmSetProperty(uint32_t change_id,
                     Id window_id,
                     const mojo::String& name,
                     mojo::Array<uint8_t> transit_data) override;
  void WmCreateTopLevelWindow(uint32_t change_id,
                              mojo::Map<mojo::String, mojo::Array<uint8_t>>
                                  transport_properties) override;
  void OnAccelerator(uint32_t id, mus::mojom::EventPtr event) override;

  // Overriden from WindowManagerClient:
  void SetFrameDecorationValues(
      mojom::FrameDecorationValuesPtr values) override;
  void AddAccelerator(uint32_t id,
                      mojom::EventMatcherPtr event_matcher,
                      const base::Callback<void(bool)>& callback) override;
  void RemoveAccelerator(uint32_t id) override;
  void AddActivationParent(Window* window) override;
  void RemoveActivationParent(Window* window) override;
  void ActivateNextWindow() override;
  void SetUnderlaySurfaceOffsetAndExtendedHitArea(
      Window* window,
      const gfx::Vector2d& offset,
      const gfx::Insets& hit_area) override;

  // This is set once and only once when we get OnEmbed(). It gives the unique
  // id for this connection.
  ConnectionSpecificId connection_id_;

  // Id assigned to the next window created.
  ConnectionSpecificId next_window_id_;

  // Id used for the next change id supplied to the server.
  uint32_t next_change_id_;
  InFlightMap in_flight_map_;

  WindowTreeDelegate* delegate_;

  WindowManagerDelegate* window_manager_delegate_;

  std::set<Window*> roots_;

  IdToWindowMap windows_;

  Window* focused_window_;

  mojo::Binding<WindowTreeClient> binding_;
  mojom::WindowTreePtr tree_ptr_;
  // Typically this is the value contained in |tree_ptr_|, but tests may
  // directly set this.
  mojom::WindowTree* tree_;

  bool is_embed_root_;

  bool delete_on_no_roots_;

  bool in_destructor_;

  base::ObserverList<WindowTreeConnectionObserver> observers_;

  scoped_ptr<mojo::AssociatedBinding<mojom::WindowManager>>
      window_manager_internal_;
  mojom::WindowManagerClientAssociatedPtr window_manager_internal_client_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(WindowTreeClientImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_TREE_CLIENT_IMPL_H_
