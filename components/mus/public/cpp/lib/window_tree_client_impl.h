// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_TREE_CLIENT_IMPL_H_
#define COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_TREE_CLIENT_IMPL_H_

#include "base/containers/scoped_ptr_map.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
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
                             public mojom::WindowTreeClient {
 public:
  WindowTreeClientImpl(WindowTreeDelegate* delegate,
                       WindowManagerDelegate* window_manager_delegate,
                       mojo::InterfaceRequest<mojom::WindowTreeClient> request);
  ~WindowTreeClientImpl() override;

  // Wait for OnEmbed(), returning when done.
  void WaitForEmbed();

  bool connected() const { return tree_ != nullptr; }
  ConnectionSpecificId connection_id() const { return connection_id_; }

  // API exposed to the window implementations that pushes local changes to the
  // service.
  void DestroyWindow(Id window_id);

  // These methods take TransportIds. For windows owned by the current
  // connection, the connection id high word can be zero. In all cases, the
  // TransportId 0x1 refers to the root window.
  void AddChild(Id child_id, Id parent_id);
  void RemoveChild(Id child_id, Id parent_id);

  void AddTransientWindow(Window* window, Id transient_window_id);
  void RemoveTransientWindowFromParent(Window* window);

  void Reorder(Id window_id,
               Id relative_window_id,
               mojom::OrderDirection direction);

  // Returns true if the specified window was created by this connection.
  bool OwnsWindow(Id id) const;

  void SetBounds(Window* window,
                 const gfx::Rect& old_bounds,
                 const gfx::Rect& bounds);
  void SetClientArea(Id window_id, const gfx::Insets& client_area);
  void SetFocus(Id window_id);
  void SetVisible(Id window_id, bool visible);
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

  void RequestSurface(Id window_id,
                      mojom::SurfaceType type,
                      mojo::InterfaceRequest<mojom::Surface> surface,
                      mojom::SurfaceClientPtr client);

  void set_change_acked_callback(const mojo::Callback<void(void)>& callback) {
    change_acked_callback_ = callback;
  }
  void ClearChangeAckedCallback() { change_acked_callback_.reset(); }

  // Start/stop tracking windows. While tracked, they can be retrieved via
  // WindowTreeConnection::GetWindowById.
  void AddWindow(Window* window);
  void RemoveWindow(Id window_id);

  bool is_embed_root() const { return is_embed_root_; }

  // Called after the root window's observers have been notified of destruction
  // (as the last step of ~Window). This ordering ensures that the Window Server
  // is torn down after the root.
  void OnRootDestroyed(Window* root);

  void SetPreferredSize(Id window_id, const gfx::Size& size);
  void SetShowState(Id window_id, mojom::ShowState show_state);
  void SetResizeBehavior(Id window_id, mojom::ResizeBehavior resize_behavior);

 private:
  friend class WindowTreeClientImplPrivate;

  typedef std::map<Id, Window*> IdToWindowMap;

  using InFlightMap = base::ScopedPtrMap<uint32_t, scoped_ptr<InFlightChange>>;

  // Returns the oldest InFlightChange that matches |change|.
  InFlightChange* GetOldestInFlightChangeMatching(const InFlightChange& change);

  uint32_t ScheduleInFlightChange(scoped_ptr<InFlightChange> change);

  // Returns true if there is an InFlightChange that matches |change|. If there
  // is an existing change SetRevertValueFrom() is invoked on it. Returns false
  // if there is no InFlightChange matching |change|.
  bool ApplyServerChangeToExistingInFlightChange(const InFlightChange& change);

  // OnEmbed() calls into this. Exposed as a separate function for testing.
  void OnEmbedImpl(mojom::WindowTree* window_tree,
                   ConnectionSpecificId connection_id,
                   mojom::WindowDataPtr root_data,
                   Id focused_window_id,
                   uint32 access_policy);

  // Overridden from WindowTreeConnection:
  Window* GetRoot() override;
  Window* GetWindowById(Id id) override;
  Window* GetFocusedWindow() override;
  Window* NewWindow() override;
  bool IsEmbedRoot() override;
  ConnectionSpecificId GetConnectionId() override;

  // Overridden from WindowTreeClient:
  void OnEmbed(ConnectionSpecificId connection_id,
               mojom::WindowDataPtr root,
               mojom::WindowTreePtr tree,
               Id focused_window_id,
               uint32_t access_policy) override;
  void OnEmbeddedAppDisconnected(Id window_id) override;
  void OnUnembed() override;
  void OnWindowBoundsChanged(Id window_id,
                             mojo::RectPtr old_bounds,
                             mojo::RectPtr new_bounds) override;
  void OnClientAreaChanged(uint32_t window_id,
                           mojo::InsetsPtr old_client_area,
                           mojo::InsetsPtr new_client_area) override;
  void OnTransientWindowAdded(uint32_t window_id,
                              uint32_t transient_window_id) override;
  void OnTransientWindowRemoved(uint32_t window_id,
                                uint32_t transient_window_id) override;
  void OnWindowViewportMetricsChanged(
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
  void OnWindowInputEvent(Id window_id,
                          mojom::EventPtr event,
                          const mojo::Callback<void()>& callback) override;
  void OnWindowFocused(Id focused_window_id) override;
  void OnChangeCompleted(uint32_t change_id, bool success) override;
  void WmSetBounds(uint32_t change_id,
                   Id window_id,
                   mojo::RectPtr transit_bounds) override;
  void WmSetProperty(uint32_t change_id,
                     Id window_id,
                     const mojo::String& name,
                     mojo::Array<uint8_t> transit_data) override;

  void OnActionCompleted(bool success);

  mojo::Callback<void(bool)> ActionCompletedCallback();

  // This is set once and only once when we get OnEmbed(). It gives the unique
  // id for this connection.
  ConnectionSpecificId connection_id_;

  // Id assigned to the next window created.
  ConnectionSpecificId next_window_id_;

  // Id used for the next change id supplied to the server.
  uint32_t next_change_id_;
  InFlightMap in_flight_map_;

  mojo::Callback<void(void)> change_acked_callback_;

  WindowTreeDelegate* delegate_;

  WindowManagerDelegate* window_manager_delegate_;

  Window* root_;

  IdToWindowMap windows_;

  Window* focused_window_;

  mojo::Binding<WindowTreeClient> binding_;
  mojom::WindowTreePtr tree_ptr_;
  // Typically this is the value contained in |tree_ptr_|, but tests may
  // directly set this.
  mojom::WindowTree* tree_;

  bool is_embed_root_;

  bool in_destructor_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(WindowTreeClientImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_TREE_CLIENT_IMPL_H_
