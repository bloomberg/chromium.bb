// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_CONNECTION_MANAGER_H_
#define COMPONENTS_VIEW_MANAGER_CONNECTION_MANAGER_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "components/view_manager/animation_runner.h"
#include "components/view_manager/event_dispatcher.h"
#include "components/view_manager/focus_controller_delegate.h"
#include "components/view_manager/ids.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "components/view_manager/server_view_delegate.h"
#include "components/view_manager/server_view_observer.h"
#include "components/view_manager/view_manager_root_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace view_manager {

class ClientConnection;
class ConnectionManagerDelegate;
class FocusController;
class ServerView;
class ViewManagerRootConnection;
class ViewManagerServiceImpl;

// ConnectionManager manages the set of connections to the ViewManager (all the
// ViewManagerServiceImpls) as well as providing the root of the hierarchy.
class ConnectionManager : public ServerViewDelegate,
                          public ServerViewObserver,
                          public FocusControllerDelegate {
 public:
  // Create when a ViewManagerServiceImpl is about to make a change. Ensures
  // clients are notified correctly.
  class ScopedChange {
   public:
    ScopedChange(ViewManagerServiceImpl* connection,
                 ConnectionManager* connection_manager,
                 bool is_delete_view);
    ~ScopedChange();

    mojo::ConnectionSpecificId connection_id() const { return connection_id_; }
    bool is_delete_view() const { return is_delete_view_; }

    // Marks the connection with the specified id as having seen a message.
    void MarkConnectionAsMessaged(mojo::ConnectionSpecificId connection_id) {
      message_ids_.insert(connection_id);
    }

    // Returns true if MarkConnectionAsMessaged(connection_id) was invoked.
    bool DidMessageConnection(mojo::ConnectionSpecificId connection_id) const {
      return message_ids_.count(connection_id) > 0;
    }

   private:
    ConnectionManager* connection_manager_;
    const mojo::ConnectionSpecificId connection_id_;
    const bool is_delete_view_;

    // See description of MarkConnectionAsMessaged/DidMessageConnection.
    std::set<mojo::ConnectionSpecificId> message_ids_;

    DISALLOW_COPY_AND_ASSIGN(ScopedChange);
  };

  explicit ConnectionManager(ConnectionManagerDelegate* delegate);
  ~ConnectionManager() override;

  // Adds a ViewManagerRoot.
  void AddRoot(ViewManagerRootConnection* root_connection);

  // Creates a new ServerView. The return value is owned by the caller, but must
  // be destroyed before ConnectionManager.
  ServerView* CreateServerView(const ViewId& id);

  // Returns the id for the next ViewManagerServiceImpl.
  mojo::ConnectionSpecificId GetAndAdvanceNextConnectionId();

  // Returns the id for the next ViewManagerRootImpl.
  uint16_t GetAndAdvanceNextRootId();

  // Invoked when a ViewManagerServiceImpl's connection encounters an error.
  void OnConnectionError(ClientConnection* connection);

  // Invoked when a ViewManagerRootBindingOwnerBase's connection encounters an
  // error or the associated Display window is closed.
  void OnRootConnectionClosed(ViewManagerRootConnection* connection);

  // See description of ViewManagerService::Embed() for details. This assumes
  // |transport_view_id| is valid.
  void EmbedAtView(mojo::ConnectionSpecificId creator_id,
                   const ViewId& view_id,
                   mojo::URLRequestPtr request);
  ViewManagerServiceImpl* EmbedAtView(
      mojo::ConnectionSpecificId creator_id,
      const ViewId& view_id,
      mojo::ViewManagerClientPtr client);

  // Invoked when an accelerator has been triggered on a view tree with the
  // provided |root|.
  void OnAccelerator(ServerView* root, mojo::EventPtr event);

  // Returns the connection by id.
  ViewManagerServiceImpl* GetConnection(
      mojo::ConnectionSpecificId connection_id);

  // Returns the View identified by |id|.
  ServerView* GetView(const ViewId& id);

  void SetFocusedView(ServerView* view);
  ServerView* GetFocusedView();

  // Returns whether |view| is a descendant of some root view but not itself a
  // root view.
  bool IsViewAttachedToRoot(const ServerView* view) const;

  // Schedules a paint for the specified region in the coordinates of |view|.
  void SchedulePaint(const ServerView* view, const gfx::Rect& bounds);

  bool IsProcessingChange() const { return current_change_ != NULL; }

  bool is_processing_delete_view() const {
    return current_change_ && current_change_->is_delete_view();
  }

  // Invoked when the ViewManagerRootImpl's display is closed.
  void OnDisplayClosed();

  // Invoked when a connection messages a client about the change. This is used
  // to avoid sending ServerChangeIdAdvanced() unnecessarily.
  void OnConnectionMessagedClient(mojo::ConnectionSpecificId id);

  // Returns true if OnConnectionMessagedClient() was invoked for id.
  bool DidConnectionMessageClient(mojo::ConnectionSpecificId id) const;

  // Returns the metrics of the viewport where the provided |view| is displayed.
  mojo::ViewportMetricsPtr GetViewportMetricsForView(const ServerView* view);

  // Returns the ViewManagerServiceImpl that has |id| as a root.
  ViewManagerServiceImpl* GetConnectionWithRoot(const ViewId& id) {
    return const_cast<ViewManagerServiceImpl*>(
        const_cast<const ConnectionManager*>(this)->GetConnectionWithRoot(id));
  }
  const ViewManagerServiceImpl* GetConnectionWithRoot(const ViewId& id) const;

  // Returns the first ancestor of |service| that is marked as an embed root.
  ViewManagerServiceImpl* GetEmbedRoot(ViewManagerServiceImpl* service);

  // ViewManagerRoot implementation helper; see mojom for details.
  bool CloneAndAnimate(const ViewId& view_id);

  // Dispatches |event| directly to the appropriate connection for |view|.
  void DispatchInputEventToView(const ServerView* view, mojo::EventPtr event);

  void OnEvent(ViewManagerRootImpl* root, mojo::EventPtr event);

  void AddAccelerator(ViewManagerRootImpl* root,
                      mojo::KeyboardCode keyboard_code,
                      mojo::EventFlags flags);

  void RemoveAccelerator(ViewManagerRootImpl* root,
                         mojo::KeyboardCode keyboard_code,
                         mojo::EventFlags flags);

  // Set IME's visibility for the specified view. If the view is not the current
  // focused view, this function will do nothing.
  void SetImeVisibility(ServerView* view, bool visible);

  // These functions trivially delegate to all ViewManagerServiceImpls, which in
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
  using ConnectionMap = std::map<mojo::ConnectionSpecificId, ClientConnection*>;
  using RootConnectionMap =
      std::map<ViewManagerRootImpl*, ViewManagerRootConnection*>;

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
  bool IsChangeSource(mojo::ConnectionSpecificId connection_id) const {
    return current_change_ && current_change_->connection_id() == connection_id;
  }

  // Callback from animation timer.
  // TODO(sky): make this real (move to a different class).
  void DoAnimation();

  // Adds |connection| to internal maps.
  void AddConnection(ClientConnection* connection);

  ViewManagerRootImpl* GetViewManagerRootByView(const ServerView* view) const;

  // Overridden from ServerViewDelegate:
  void PrepareToDestroyView(ServerView* view) override;
  void PrepareToChangeViewHierarchy(ServerView* view,
                                    ServerView* new_parent,
                                    ServerView* old_parent) override;
  void PrepareToChangeViewVisibility(ServerView* view) override;
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

  void CloneAndAnimate(mojo::Id transport_view_id);

  // FocusControllerDelegate:
  void OnFocusChanged(ServerView* old_focused_view,
                      ServerView* new_focused_view) override;

  ConnectionManagerDelegate* delegate_;

  // ID to use for next ViewManagerServiceImpl.
  mojo::ConnectionSpecificId next_connection_id_;

  // ID to use for next ViewManagerRootImpl.
  uint16_t next_root_id_;

  EventDispatcher event_dispatcher_;

  // Set of ViewManagerServiceImpls.
  ConnectionMap connection_map_;

  // Set of ViewManagerRootImpls.
  RootConnectionMap root_connection_map_;

  // If non-null we're processing a change. The ScopedChange is not owned by us
  // (it's created on the stack by ViewManagerServiceImpl).
  ScopedChange* current_change_;

  bool in_destructor_;

  // TODO(sky): nuke! Just a proof of concept until get real animation api.
  base::RepeatingTimer<ConnectionManager> animation_timer_;

  AnimationRunner animation_runner_;

  scoped_ptr<FocusController> focus_controller_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_CONNECTION_MANAGER_H_
