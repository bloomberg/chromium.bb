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
#include "components/view_manager/display_manager.h"
#include "components/view_manager/event_dispatcher.h"
#include "components/view_manager/focus_controller_delegate.h"
#include "components/view_manager/ids.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "components/view_manager/server_view_delegate.h"
#include "components/view_manager/server_view_observer.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace view_manager {

class ClientConnection;
class ConnectionManagerDelegate;
class FocusController;
class ServerView;
class ViewManagerServiceImpl;

// ConnectionManager manages the set of connections to the ViewManager (all the
// ViewManagerServiceImpls) as well as providing the root of the hierarchy.
class ConnectionManager : public ServerViewDelegate,
                          public ServerViewObserver,
                          public mojo::ViewManagerRoot,
                          public FocusControllerDelegate,
                          public DisplayManagerDelegate {
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

  ConnectionManager(ConnectionManagerDelegate* delegate,
                    bool is_headless,
                    mojo::ApplicationImpl* app_impl,
                    const scoped_refptr<gles2::GpuState>& gpu_state);
  ~ConnectionManager() override;

  // Creates a new ServerView. The return value is owned by the caller, but must
  // be destroyed before ConnectionManager.
  ServerView* CreateServerView(const ViewId& id);

  // Returns the id for the next ViewManagerServiceImpl.
  mojo::ConnectionSpecificId GetAndAdvanceNextConnectionId();

  // Invoked when a ViewManagerServiceImpl's connection encounters an error.
  void OnConnectionError(ClientConnection* connection);

  // See description of ViewManagerService::Embed() for details. This assumes
  // |transport_view_id| is valid.
  void EmbedAtView(mojo::ConnectionSpecificId creator_id,
                   const ViewId& view_id,
                   mojo::URLRequestPtr request);
  void EmbedAtView(mojo::ConnectionSpecificId creator_id,
                   const ViewId& view_id,
                   mojo::ViewManagerClientPtr client);

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

  DisplayManager* display_manager() { return display_manager_.get(); }

  bool IsProcessingChange() const { return current_change_ != NULL; }

  bool is_processing_delete_view() const {
    return current_change_ && current_change_->is_delete_view();
  }

  // Invoked when a connection messages a client about the change. This is used
  // to avoid sending ServerChangeIdAdvanced() unnecessarily.
  void OnConnectionMessagedClient(mojo::ConnectionSpecificId id);

  // Returns true if OnConnectionMessagedClient() was invoked for id.
  bool DidConnectionMessageClient(mojo::ConnectionSpecificId id) const;

  // Returns the ViewManagerServiceImpl that has |id| as a root.
  ViewManagerServiceImpl* GetConnectionWithRoot(const ViewId& id) {
    return const_cast<ViewManagerServiceImpl*>(
        const_cast<const ConnectionManager*>(this)->GetConnectionWithRoot(id));
  }
  const ViewManagerServiceImpl* GetConnectionWithRoot(const ViewId& id) const;

  // Returns the first ancestor of |service| that is marked as an embed root.
  ViewManagerServiceImpl* GetEmbedRoot(ViewManagerServiceImpl* service);

  mojo::ViewManagerRootClient* view_manager_root_client() {
    return view_manager_root_client_.get();
  }

  void SetWindowManagerClientConnection(
      scoped_ptr<ClientConnection> connection);
  bool has_window_manager_client_connection() const {
    return window_manager_client_connection_ != nullptr;
  }

  mojo::ViewManagerClient* GetWindowManagerViewManagerClient();

  // ViewManagerRoot implementation helper; see mojom for details.
  bool CloneAndAnimate(const ViewId& view_id);

  // Processes an event, potentially changing focus.
  void ProcessEvent(mojo::EventPtr event);

  // Dispatches |event| directly to the appropriate connection for |view|.
  void DispatchInputEventToView(const ServerView* view, mojo::EventPtr event);

  // DisplayManager::Delegate:
  ServerView* GetRoot() override;
  void OnEvent(mojo::EventPtr event) override;
  void OnDisplayClosed() override;
  void OnViewportMetricsChanged(
      const mojo::ViewportMetrics& old_metrics,
      const mojo::ViewportMetrics& new_metrics) override;

  // These functions trivially delegate to all ViewManagerServiceImpls, which in
  // term notify their clients.
  void ProcessViewDestroyed(ServerView* view);
  void ProcessViewBoundsChanged(const ServerView* view,
                                const gfx::Rect& old_bounds,
                                const gfx::Rect& new_bounds);
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

  // Overridden from ServerViewDelegate:
  void PrepareToDestroyView(ServerView* view) override;
  void PrepareToChangeViewHierarchy(ServerView* view,
                                    ServerView* new_parent,
                                    ServerView* old_parent) override;
  void PrepareToChangeViewVisibility(ServerView* view) override;
  void OnScheduleViewPaint(const ServerView* view) override;
  bool IsViewDrawn(const ServerView* view) const override;

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

  // ViewManagerRoot:
  void SetViewManagerRootClient(mojo::ViewManagerRootClientPtr client) override;
  void SetViewportSize(mojo::SizePtr size) override;
  void CloneAndAnimate(mojo::Id transport_view_id) override;
  void AddAccelerator(mojo::KeyboardCode keyboard_code,
                      mojo::EventFlags flags) override;
  void RemoveAccelerator(mojo::KeyboardCode keyboard_code,
                         mojo::EventFlags flags) override;

  // FocusControllerDelegate:
  void OnFocusChanged(ServerView* old_focused_view,
                      ServerView* new_focused_view) override;

  ConnectionManagerDelegate* delegate_;

  // The ClientConnection containing the ViewManagerService implementation
  // provided to the initial connection (the WindowManager).
  // NOTE: |window_manager_client_connection_| is also in |connection_map_|.
  ClientConnection* window_manager_client_connection_;

  // ID to use for next ViewManagerServiceImpl.
  mojo::ConnectionSpecificId next_connection_id_;

  // Set of ViewManagerServiceImpls.
  ConnectionMap connection_map_;

  // DisplayManager holds a raw pointer to EventDispatcher and so it must be
  // destroyed after DisplayManager (and thus created before).
  EventDispatcher event_dispatcher_;

  scoped_ptr<DisplayManager> display_manager_;

  scoped_ptr<ServerView> root_;

  // If non-null we're processing a change. The ScopedChange is not owned by us
  // (it's created on the stack by ViewManagerServiceImpl).
  ScopedChange* current_change_;

  bool in_destructor_;

  // TODO(sky): nuke! Just a proof of concept until get real animation api.
  base::RepeatingTimer<ConnectionManager> animation_timer_;

  AnimationRunner animation_runner_;

  scoped_ptr<FocusController> focus_controller_;

  mojo::ViewManagerRootClientPtr view_manager_root_client_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_CONNECTION_MANAGER_H_
