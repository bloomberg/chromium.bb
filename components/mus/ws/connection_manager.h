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
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "components/mus/public/interfaces/view_tree_host.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/focus_controller_delegate.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/server_view_delegate.h"
#include "components/mus/ws/server_view_observer.h"
#include "components/mus/ws/view_tree_host_impl.h"
#include "mojo/converters/surfaces/custom_surface_converter.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mus {

class ClientConnection;
class ConnectionManagerDelegate;
class ServerView;
class ViewTreeHostConnection;
class ViewTreeImpl;

// ConnectionManager manages the set of connections to the ViewManager (all the
// ViewTreeImpl) as well as providing the root of the hierarchy.
class ConnectionManager : public ServerViewDelegate, public ServerViewObserver {
 public:
  // Create when a ViewTreeImpl is about to make a change. Ensures clients are
  // notified correctly.
  class ScopedChange {
   public:
    ScopedChange(ViewTreeImpl* connection,
                 ConnectionManager* connection_manager,
                 bool is_delete_view);
    ~ScopedChange();

    ConnectionSpecificId connection_id() const { return connection_id_; }
    bool is_delete_view() const { return is_delete_view_; }

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
    const bool is_delete_view_;

    // See description of MarkConnectionAsMessaged/DidMessageConnection.
    std::set<ConnectionSpecificId> message_ids_;

    DISALLOW_COPY_AND_ASSIGN(ScopedChange);
  };

  ConnectionManager(ConnectionManagerDelegate* delegate,
                    const scoped_refptr<SurfacesState>& surfaces_state);
  ~ConnectionManager() override;

  // Adds a ViewTreeHost.
  void AddHost(ViewTreeHostConnection* connection);

  // Creates a new ServerView. The return value is owned by the caller, but must
  // be destroyed before ConnectionManager.
  ServerView* CreateServerView(const ViewId& id);

  // Returns the id for the next ViewTreeImpl.
  ConnectionSpecificId GetAndAdvanceNextConnectionId();

  // Returns the id for the next ViewTreeHostImpl.
  uint16_t GetAndAdvanceNextHostId();

  // Invoked when a ViewTreeImpl's connection encounters an error.
  void OnConnectionError(ClientConnection* connection);

  // Invoked when a ViewTreeHostBindingOwnerBase's connection encounters an
  // error or the associated Display window is closed.
  void OnHostConnectionClosed(ViewTreeHostConnection* connection);

  // See description of ViewTree::Embed() for details. This assumes
  // |transport_view_id| is valid.
  void EmbedAtView(ConnectionSpecificId creator_id,
                   const ViewId& view_id,
                   uint32_t policy_bitmask,
                   mojo::URLRequestPtr request);
  ViewTreeImpl* EmbedAtView(ConnectionSpecificId creator_id,
                            const ViewId& view_id,
                            uint32_t policy_bitmask,
                            mojo::ViewTreeClientPtr client);

  // Returns the connection by id.
  ViewTreeImpl* GetConnection(ConnectionSpecificId connection_id);

  // Returns the View identified by |id|.
  ServerView* GetView(const ViewId& id);

  // Returns whether |view| is a descendant of some root view but not itself a
  // root view.
  bool IsViewAttachedToRoot(const ServerView* view) const;

  // Schedules a paint for the specified region in the coordinates of |view|.
  void SchedulePaint(const ServerView* view, const gfx::Rect& bounds);

  bool IsProcessingChange() const { return current_change_ != NULL; }

  bool is_processing_delete_view() const {
    return current_change_ && current_change_->is_delete_view();
  }

  // Invoked when the ViewTreeHostImpl's display is closed.
  void OnDisplayClosed();

  // Invoked when a connection messages a client about the change. This is used
  // to avoid sending ServerChangeIdAdvanced() unnecessarily.
  void OnConnectionMessagedClient(ConnectionSpecificId id);

  // Returns true if OnConnectionMessagedClient() was invoked for id.
  bool DidConnectionMessageClient(ConnectionSpecificId id) const;

  // Returns the metrics of the viewport where the provided |view| is displayed.
  mojo::ViewportMetricsPtr GetViewportMetricsForView(const ServerView* view);

  // Returns the ViewTreeImpl that has |id| as a root.
  ViewTreeImpl* GetConnectionWithRoot(const ViewId& id) {
    return const_cast<ViewTreeImpl*>(
        const_cast<const ConnectionManager*>(this)->GetConnectionWithRoot(id));
  }
  const ViewTreeImpl* GetConnectionWithRoot(const ViewId& id) const;

  ViewTreeHostImpl* GetViewTreeHostByView(const ServerView* view);
  const ViewTreeHostImpl* GetViewTreeHostByView(const ServerView* view) const;

  // Returns the first ancestor of |service| that is marked as an embed root.
  ViewTreeImpl* GetEmbedRoot(ViewTreeImpl* service);

  // ViewTreeHost implementation helper; see mojom for details.
  bool CloneAndAnimate(const ViewId& view_id);

  // These functions trivially delegate to all ViewTreeImpls, which in
  // term notify their clients.
  void ProcessViewDestroyed(ServerView* view);
  void ProcessViewBoundsChanged(const ServerView* view,
                                const gfx::Rect& old_bounds,
                                const gfx::Rect& new_bounds);
  void ProcessViewportMetricsChanged(const mojo::ViewportMetrics& old_metrics,
                                     const mojo::ViewportMetrics& new_metrics);
  void ProcessWillChangeViewHierarchy(const ServerView* view,
                                      const ServerView* new_parent,
                                      const ServerView* old_parent);
  void ProcessViewHierarchyChanged(const ServerView* view,
                                   const ServerView* new_parent,
                                   const ServerView* old_parent);
  void ProcessViewReorder(const ServerView* view,
                          const ServerView* relative_view,
                          const mojo::OrderDirection direction);
  void ProcessViewDeleted(const ViewId& view);

 private:
  using ConnectionMap = std::map<ConnectionSpecificId, ClientConnection*>;
  using HostConnectionMap =
      std::map<ViewTreeHostImpl*, ViewTreeHostConnection*>;

  // Invoked when a connection is about to make a change.  Subsequently followed
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

  // Overridden from ServerViewDelegate:
  SurfacesState* GetSurfacesState() override;
  void OnScheduleViewPaint(const ServerView* view) override;
  const ServerView* GetRootView(const ServerView* view) const override;

  // Overridden from ServerViewObserver:
  void OnViewDestroyed(ServerView* view) override;
  void OnWillChangeViewHierarchy(ServerView* view,
                                 ServerView* new_parent,
                                 ServerView* old_parent) override;
  void OnViewHierarchyChanged(ServerView* view,
                              ServerView* new_parent,
                              ServerView* old_parent) override;
  void OnViewBoundsChanged(ServerView* view,
                           const gfx::Rect& old_bounds,
                           const gfx::Rect& new_bounds) override;
  void OnViewReordered(ServerView* view,
                       ServerView* relative,
                       mojo::OrderDirection direction) override;
  void OnWillChangeViewVisibility(ServerView* view) override;
  void OnViewSharedPropertyChanged(
      ServerView* view,
      const std::string& name,
      const std::vector<uint8_t>* new_data) override;
  void OnViewTextInputStateChanged(ServerView* view,
                                   const ui::TextInputState& state) override;

  ConnectionManagerDelegate* delegate_;

  // State for rendering into a Surface.
  scoped_refptr<SurfacesState> surfaces_state_;

  // ID to use for next ViewTreeImpl.
  ConnectionSpecificId next_connection_id_;

  // ID to use for next ViewTreeHostImpl.
  uint16_t next_host_id_;

  // Set of ViewTreeImpls.
  ConnectionMap connection_map_;

  // Set of ViewTreeHostImpls.
  HostConnectionMap host_connection_map_;

  // If non-null we're processing a change. The ScopedChange is not owned by us
  // (it's created on the stack by ViewTreeImpl).
  ScopedChange* current_change_;

  bool in_destructor_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_CONNECTION_MANAGER_H_
