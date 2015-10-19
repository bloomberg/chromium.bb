// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_TREE_IMPL_H_
#define COMPONENTS_MUS_WS_WINDOW_TREE_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/surface_id.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/ws/access_policy_delegate.h"
#include "components/mus/ws/ids.h"

namespace gfx {
class Rect;
}

namespace mus {

class AccessPolicy;
class ConnectionManager;
class ServerWindow;
class WindowTreeHostImpl;

// An instance of WindowTreeImpl is created for every WindowTree request.
// WindowTreeImpl tracks all the state and windows created by a client.
// WindowTreeImpl
// coordinates with ConnectionManager to update the client (and internal state)
// as necessary.
class WindowTreeImpl : public mojom::WindowTree, public AccessPolicyDelegate {
 public:
  WindowTreeImpl(ConnectionManager* connection_manager,
                 ConnectionSpecificId creator_id,
                 const WindowId& root_id,
                 uint32_t policy_bitmask);
  ~WindowTreeImpl() override;

  // |services| and |exposed_services| are the ServiceProviders to pass to the
  // client via OnEmbed().
  void Init(mojom::WindowTreeClient* client, mojom::WindowTreePtr tree);

  ConnectionSpecificId id() const { return id_; }
  ConnectionSpecificId creator_id() const { return creator_id_; }

  mojom::WindowTreeClient* client() { return client_; }

  // Returns the Window with the specified id.
  ServerWindow* GetWindow(const WindowId& id) {
    return const_cast<ServerWindow*>(
        const_cast<const WindowTreeImpl*>(this)->GetWindow(id));
  }
  const ServerWindow* GetWindow(const WindowId& id) const;

  // Returns true if this connection's root is |id|.
  bool IsRoot(const WindowId& id) const;

  // Returns the id of the root node. This is null if the root has been
  // destroyed but the connection is still valid.
  const WindowId* root() const { return root_.get(); }

  bool is_embed_root() const { return is_embed_root_; }

  WindowTreeHostImpl* GetHost();

  // Invoked when a connection is about to be destroyed.
  void OnWillDestroyWindowTreeImpl(WindowTreeImpl* connection);

  // These functions are synchronous variants of those defined in the mojom. The
  // WindowTree implementations all call into these. See the mojom for details.
  mojom::ErrorCode NewWindow(const WindowId& window_id);
  bool AddWindow(const WindowId& parent_id, const WindowId& child_id);
  std::vector<const ServerWindow*> GetWindowTree(
      const WindowId& window_id) const;
  bool SetWindowVisibility(const WindowId& window_id, bool visible);
  bool Embed(const WindowId& window_id,
             mojom::WindowTreeClientPtr client,
             uint32_t policy_bitmask,
             ConnectionSpecificId* connection_id);
  void Embed(const WindowId& window_id, mojo::URLRequestPtr request);

  // The following methods are invoked after the corresponding change has been
  // processed. They do the appropriate bookkeeping and update the client as
  // necessary.
  void ProcessWindowBoundsChanged(const ServerWindow* window,
                                  const gfx::Rect& old_bounds,
                                  const gfx::Rect& new_bounds,
                                  bool originated_change);
  void ProcessClientAreaChanged(const ServerWindow* window,
                                const gfx::Rect& old_client_area,
                                const gfx::Rect& new_client_area,
                                bool originated_change);
  void ProcessViewportMetricsChanged(const mojom::ViewportMetrics& old_metrics,
                                     const mojom::ViewportMetrics& new_metrics,
                                     bool originated_change);
  void ProcessWillChangeWindowHierarchy(const ServerWindow* window,
                                        const ServerWindow* new_parent,
                                        const ServerWindow* old_parent,
                                        bool originated_change);
  void ProcessWindowPropertyChanged(const ServerWindow* window,
                                    const std::string& name,
                                    const std::vector<uint8_t>* new_data,
                                    bool originated_change);
  void ProcessWindowHierarchyChanged(const ServerWindow* window,
                                     const ServerWindow* new_parent,
                                     const ServerWindow* old_parent,
                                     bool originated_change);
  void ProcessWindowReorder(const ServerWindow* window,
                            const ServerWindow* relative_window,
                            mojom::OrderDirection direction,
                            bool originated_change);
  void ProcessWindowDeleted(const WindowId& window, bool originated_change);
  void ProcessWillChangeWindowVisibility(const ServerWindow* window,
                                         bool originated_change);
  void ProcessFocusChanged(const ServerWindow* old_focused_window,
                           const ServerWindow* new_focused_window);

 private:
  using WindowIdSet = base::hash_set<Id>;
  using WindowMap = std::map<ConnectionSpecificId, ServerWindow*>;

  bool IsWindowKnown(const ServerWindow* window) const;

  // These functions return true if the corresponding mojom function is allowed
  // for this connection.
  bool CanReorderWindow(const ServerWindow* window,
                        const ServerWindow* relative_window,
                        mojom::OrderDirection direction) const;

  // Deletes a window owned by this connection. Returns true on success.
  // |source|
  // is the connection that originated the change.
  bool DeleteWindowImpl(WindowTreeImpl* source, ServerWindow* window);

  // If |window| is known (in |known_windows_|) does nothing. Otherwise adds
  // |window|
  // to |windows|, marks |window| as known and recurses.
  void GetUnknownWindowsFrom(const ServerWindow* window,
                             std::vector<const ServerWindow*>* windows);

  // Removes |window| and all its descendants from |known_windows_|. This does
  // not
  // recurse through windows that were created by this connection. All windows
  // owned
  // by this connection are added to |local_windows|.
  void RemoveFromKnown(const ServerWindow* window,
                       std::vector<ServerWindow*>* local_windows);

  // Resets the root of this connection.
  void RemoveRoot();

  // Converts Window(s) to WindowData(s) for transport. This assumes all the
  // windows are valid for the client. The parent of windows the client is not
  // allowed to see are set to NULL (in the returned WindowData(s)).
  mojo::Array<mojom::WindowDataPtr> WindowsToWindowDatas(
      const std::vector<const ServerWindow*>& windows);
  mojom::WindowDataPtr WindowToWindowData(const ServerWindow* window);

  // Implementation of GetWindowTree(). Adds |window| to |windows| and recurses
  // if
  // CanDescendIntoWindowForWindowTree() returns true.
  void GetWindowTreeImpl(const ServerWindow* window,
                         std::vector<const ServerWindow*>* windows) const;

  // Notify the client if the drawn state of any of the roots changes.
  // |window| is the window that is changing to the drawn state
  // |new_drawn_value|.
  void NotifyDrawnStateChanged(const ServerWindow* window,
                               bool new_drawn_value);

  // Deletes all Windows we own.
  void DestroyWindows();

  bool CanEmbed(const WindowId& window_id, uint32_t policy_bitmask) const;
  void PrepareForEmbed(const WindowId& window_id);
  void RemoveChildrenAsPartOfEmbed(const WindowId& window_id);

  // WindowTree:
  void NewWindow(
      Id transport_window_id,
      const mojo::Callback<void(mojom::ErrorCode)>& callback) override;
  void DeleteWindow(Id transport_window_id,
                    const mojo::Callback<void(bool)>& callback) override;
  void AddWindow(Id parent_id,
                 Id child_id,
                 const mojo::Callback<void(bool)>& callback) override;
  void RemoveWindowFromParent(
      Id window_id,
      const mojo::Callback<void(bool)>& callback) override;
  void ReorderWindow(Id window_id,
                     Id relative_window_id,
                     mojom::OrderDirection direction,
                     const mojo::Callback<void(bool)>& callback) override;
  void GetWindowTree(
      Id window_id,
      const mojo::Callback<void(mojo::Array<mojom::WindowDataPtr>)>& callback)
      override;
  void SetWindowBounds(Id window_id,
                       mojo::RectPtr bounds,
                       const mojo::Callback<void(bool)>& callback) override;
  void SetWindowVisibility(Id window_id,
                           bool visible,
                           const mojo::Callback<void(bool)>& callback) override;
  void SetWindowProperty(Id window_id,
                         const mojo::String& name,
                         mojo::Array<uint8_t> value,
                         const mojo::Callback<void(bool)>& callback) override;
  void RequestSurface(Id window_id,
                      mojo::InterfaceRequest<mojom::Surface> surface,
                      mojom::SurfaceClientPtr client) override;
  void Embed(Id transport_window_id,
             mojom::WindowTreeClientPtr client,
             uint32_t policy_bitmask,
             const EmbedCallback& callback) override;
  void SetFocus(uint32_t window_id) override;
  void SetWindowTextInputState(uint32_t window_id,
                               mojo::TextInputStatePtr state) override;
  void SetImeVisibility(Id transport_window_id,
                        bool visible,
                        mojo::TextInputStatePtr state) override;
  void SetClientArea(Id transport_window_id, mojo::RectPtr rect) override;

  // AccessPolicyDelegate:
  bool IsRootForAccessPolicy(const WindowId& id) const override;
  bool IsWindowKnownForAccessPolicy(const ServerWindow* window) const override;
  bool IsWindowRootOfAnotherConnectionForAccessPolicy(
      const ServerWindow* window) const override;
  bool IsDescendantOfEmbedRoot(const ServerWindow* window) override;

  ConnectionManager* connection_manager_;

  // Id of this connection as assigned by ConnectionManager.
  const ConnectionSpecificId id_;

  // ID of the connection that created us. If 0 it indicates either we were
  // created by the root, or the connection that created us has been destroyed.
  ConnectionSpecificId creator_id_;

  mojom::WindowTreeClient* client_;

  scoped_ptr<mus::AccessPolicy> access_policy_;

  // The windows created by this connection. This connection owns these objects.
  WindowMap window_map_;

  // The set of windows that has been communicated to the client.
  WindowIdSet known_windows_;

  // The root of this connection. This is a scoped_ptr to reinforce the
  // connection may have no root. A connection has no root if either the root
  // is destroyed or Embed() is invoked on the root.
  scoped_ptr<WindowId> root_;

  bool is_embed_root_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_TREE_IMPL_H_
