// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/connection_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/view_manager/client_connection.h"
#include "components/view_manager/connection_manager_delegate.h"
#include "components/view_manager/display_manager.h"
#include "components/view_manager/focus_controller.h"
#include "components/view_manager/server_view.h"
#include "components/view_manager/view_coordinate_conversions.h"
#include "components/view_manager/view_manager_service_impl.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"

using mojo::ConnectionSpecificId;

namespace view_manager {
namespace {

// Creates a copy of |view|. The copied view has |delegate| as its delegate.
// This does not recurse.
ServerView* CloneView(const ServerView* view, ServerViewDelegate* delegate) {
  ServerView* clone = new ServerView(delegate, ClonedViewId());
  clone->SetBounds(view->bounds());
  clone->SetSurfaceId(view->surface_id());
  clone->SetOpacity(view->opacity());
  return clone;
}

// Creates copies of all the visible children of |parent|. Newly cloned views
// are added to |cloned_parent| and have |delegate| as their delegate. The
// stacking order of the cloned views is preseved.
void CloneViewTree(const ServerView* parent,
                   ServerView* cloned_parent,
                   ServerViewDelegate* delegate) {
  DCHECK(parent->visible());
  for (const ServerView* to_clone : parent->GetChildren()) {
    if (to_clone->visible()) {
      ServerView* cloned = CloneView(to_clone, delegate);
      cloned_parent->Add(cloned);
      CloneViewTree(to_clone, cloned, delegate);
    }
  }
}

// Recurses through all the children of |view| moving any cloned views to
// |new_parent| stacked above |stack_above|. |stack_above| is updated as views
// are moved.
void ReparentClonedViews(ServerView* new_parent,
                         ServerView** stack_above,
                         ServerView* view) {
  if (view->id() == ClonedViewId()) {
    const gfx::Rect new_bounds(ConvertRectBetweenViews(
        view, new_parent, gfx::Rect(view->bounds().size())));
    new_parent->Add(view);
    new_parent->Reorder(view, *stack_above, mojo::ORDER_DIRECTION_ABOVE);
    view->SetBounds(new_bounds);
    *stack_above = view;
    return;
  }

  for (ServerView* child : view->GetChildren())
    ReparentClonedViews(new_parent, stack_above, child);
}

// Deletes |view| and all its descendants.
void DeleteViewTree(ServerView* view) {
  for (ServerView* child : view->GetChildren())
    DeleteViewTree(child);

  delete view;
}

// TODO(sky): nuke, proof of concept.
bool DecrementAnimatingViewsOpacity(ServerView* view) {
  if (view->id() == ClonedViewId()) {
    const float new_opacity = view->opacity() - .05f;
    if (new_opacity <= 0)
      DeleteViewTree(view);
    else
      view->SetOpacity(new_opacity);
    return true;
  }
  bool ret_value = false;
  for (ServerView* child : view->GetChildren()) {
    if (DecrementAnimatingViewsOpacity(child))
      ret_value = true;
  }
  return ret_value;
}

}  // namespace

ConnectionManager::ScopedChange::ScopedChange(
    ViewManagerServiceImpl* connection,
    ConnectionManager* connection_manager,
    bool is_delete_view)
    : connection_manager_(connection_manager),
      connection_id_(connection->id()),
      is_delete_view_(is_delete_view) {
  connection_manager_->PrepareForChange(this);
}

ConnectionManager::ScopedChange::~ScopedChange() {
  connection_manager_->FinishChange();
}

ConnectionManager::ConnectionManager(
    ConnectionManagerDelegate* delegate,
    bool is_headless,
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<gles2::GpuState>& gpu_state)
    : delegate_(delegate),
      window_manager_client_connection_(nullptr),
      next_connection_id_(1),
      event_dispatcher_(this),
      display_manager_(
          DisplayManager::Create(is_headless, app_impl, gpu_state)),
      root_(CreateServerView(RootViewId(0))),
      current_change_(nullptr),
      in_destructor_(false),
      animation_runner_(base::TimeTicks::Now()),
      focus_controller_(new FocusController(this)) {
  root_->SetBounds(gfx::Rect(800, 600));
  root_->SetVisible(true);

  display_manager_->Init(this);
}

ConnectionManager::~ConnectionManager() {
  in_destructor_ = true;

  // Deleting views will attempt to advance focus. When we're being destroyed
  // that is not necessary. Additionally |focus_controller_| needs to be
  // destroyed before |root_|.
  focus_controller_.reset();

  STLDeleteValues(&connection_map_);
  // All the connections should have been destroyed.
  DCHECK(connection_map_.empty());
  root_.reset();
}

ServerView* ConnectionManager::CreateServerView(const ViewId& id) {
  ServerView* view = new ServerView(this, id);
  view->AddObserver(this);
  return view;
}

ConnectionSpecificId ConnectionManager::GetAndAdvanceNextConnectionId() {
  const ConnectionSpecificId id = next_connection_id_++;
  DCHECK_LT(id, next_connection_id_);
  return id;
}

void ConnectionManager::OnConnectionError(ClientConnection* connection) {
  if (connection == window_manager_client_connection_) {
    window_manager_client_connection_ = nullptr;
    delegate_->OnLostConnectionToWindowManager();
    // Assume we've been destroyed.
    return;
  }

  scoped_ptr<ClientConnection> connection_owner(connection);

  connection_map_.erase(connection->service()->id());

  // TODO(sky): I may want to advance focus differently if focus is in
  // |connection|.

  // Notify remaining connections so that they can cleanup.
  for (auto& pair : connection_map_) {
    pair.second->service()->OnWillDestroyViewManagerServiceImpl(
        connection->service());
  }
}

void ConnectionManager::EmbedAtView(mojo::ConnectionSpecificId creator_id,
                                    const ViewId& view_id,
                                    mojo::URLRequestPtr request) {
  mojo::ViewManagerServicePtr service_ptr;
  ClientConnection* client_connection =
      delegate_->CreateClientConnectionForEmbedAtView(
          this, GetProxy(&service_ptr), creator_id, request.Pass(), view_id);
  AddConnection(client_connection);
  client_connection->service()->Init(client_connection->client(),
                                     service_ptr.Pass());
  OnConnectionMessagedClient(client_connection->service()->id());
}

void ConnectionManager::EmbedAtView(mojo::ConnectionSpecificId creator_id,
                                    const ViewId& view_id,
                                    mojo::ViewManagerClientPtr client) {
  mojo::ViewManagerServicePtr service_ptr;
  ClientConnection* client_connection =
      delegate_->CreateClientConnectionForEmbedAtView(
          this, GetProxy(&service_ptr), creator_id, view_id, client.Pass());
  AddConnection(client_connection);
  client_connection->service()->Init(client_connection->client(),
                                     service_ptr.Pass());
  OnConnectionMessagedClient(client_connection->service()->id());
}

ViewManagerServiceImpl* ConnectionManager::GetConnection(
    ConnectionSpecificId connection_id) {
  ConnectionMap::iterator i = connection_map_.find(connection_id);
  return i == connection_map_.end() ? nullptr : i->second->service();
}

ServerView* ConnectionManager::GetView(const ViewId& id) {
  if (id == root_->id())
    return root_.get();
  ViewManagerServiceImpl* service = GetConnection(id.connection_id);
  return service ? service->GetView(id) : nullptr;
}

void ConnectionManager::SetFocusedView(ServerView* view) {
  ServerView* old_focused = GetFocusedView();
  if (old_focused == view)
    return;
  focus_controller_->SetFocusedView(view);
  OnFocusChanged(old_focused, view);
}

ServerView* ConnectionManager::GetFocusedView() {
  return focus_controller_->GetFocusedView();
}

bool ConnectionManager::IsViewAttachedToRoot(const ServerView* view) const {
  // TODO(fsamuel): Iterate over all roots once we support multiple roots.
  return root_->Contains(view) && view != root_.get();
}

void ConnectionManager::OnConnectionMessagedClient(ConnectionSpecificId id) {
  if (current_change_)
    current_change_->MarkConnectionAsMessaged(id);
}

bool ConnectionManager::DidConnectionMessageClient(
    ConnectionSpecificId id) const {
  return current_change_ && current_change_->DidMessageConnection(id);
}

const ViewManagerServiceImpl* ConnectionManager::GetConnectionWithRoot(
    const ViewId& id) const {
  for (auto& pair : connection_map_) {
    if (pair.second->service()->IsRoot(id))
      return pair.second->service();
  }
  return nullptr;
}

ViewManagerServiceImpl* ConnectionManager::GetEmbedRoot(
    ViewManagerServiceImpl* service) {
  while (service) {
    const ViewId* root_id = service->root();
    if (!root_id || root_id->connection_id == service->id())
      return nullptr;

    ViewManagerServiceImpl* parent_service =
        GetConnection(root_id->connection_id);
    service = parent_service;
    if (service && service->is_embed_root())
      return service;
  }
  return nullptr;
}

void ConnectionManager::SetWindowManagerClientConnection(
    scoped_ptr<ClientConnection> connection) {
  CHECK(!window_manager_client_connection_);
  window_manager_client_connection_ = connection.release();
  AddConnection(window_manager_client_connection_);
  window_manager_client_connection_->service()->Init(
      window_manager_client_connection_->client(), nullptr);
}

mojo::ViewManagerClient*
ConnectionManager::GetWindowManagerViewManagerClient() {
  CHECK(window_manager_client_connection_);
  return window_manager_client_connection_->client();
}

bool ConnectionManager::CloneAndAnimate(const ViewId& view_id) {
  ServerView* view = GetView(view_id);
  if (!view || !view->IsDrawn() || view == root_.get())
    return false;
  if (!animation_timer_.IsRunning()) {
    animation_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(100),
                           this, &ConnectionManager::DoAnimation);
  }
  ServerView* clone = CloneView(view, this);
  CloneViewTree(view, clone, this);
  view->parent()->Add(clone);
  view->parent()->Reorder(clone, view, mojo::ORDER_DIRECTION_ABOVE);
  return true;
}

void ConnectionManager::ProcessEvent(mojo::EventPtr event) {
  event_dispatcher_.OnEvent(event.Pass());
}

void ConnectionManager::DispatchInputEventToView(const ServerView* view,
                                                 mojo::EventPtr event) {
  // It's possible for events to flow through here from the platform_window
  // before any connections are established with the view_manager.
  if (!has_window_manager_client_connection())
    return;

  // If the view is an embed root, forward to the embedded view, not the owner.
  ViewManagerServiceImpl* connection = GetConnectionWithRoot(view->id());
  if (!connection)
    connection = GetConnection(view->id().connection_id);
  CHECK(connection);
  connection->client()->OnViewInputEvent(ViewIdToTransportId(view->id()),
                                         event.Pass(),
                                         base::Bind(&base::DoNothing));
}

ServerView* ConnectionManager::GetRoot() {
  return root_.get();
}

void ConnectionManager::OnEvent(mojo::EventPtr event) {
  event_dispatcher_.OnEvent(event.Pass());
}

void ConnectionManager::OnDisplayClosed() {
  delegate_->OnLostConnectionToWindowManager();
}

void ConnectionManager::OnViewportMetricsChanged(
    const mojo::ViewportMetrics& old_metrics,
    const mojo::ViewportMetrics& new_metrics) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewportMetricsChanged(
        old_metrics, new_metrics, IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessViewBoundsChanged(const ServerView* view,
                                                 const gfx::Rect& old_bounds,
                                                 const gfx::Rect& new_bounds) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewBoundsChanged(
        view, old_bounds, new_bounds, IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessWillChangeViewHierarchy(
    const ServerView* view,
    const ServerView* new_parent,
    const ServerView* old_parent) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWillChangeViewHierarchy(
        view, new_parent, old_parent, IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessViewHierarchyChanged(
    const ServerView* view,
    const ServerView* new_parent,
    const ServerView* old_parent) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewHierarchyChanged(
        view, new_parent, old_parent, IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessViewReorder(
    const ServerView* view,
    const ServerView* relative_view,
    const mojo::OrderDirection direction) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewReorder(view, relative_view, direction,
                                               IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessViewDeleted(const ViewId& view) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewDeleted(view,
                                               IsChangeSource(pair.first));
  }
}

void ConnectionManager::PrepareForChange(ScopedChange* change) {
  // Should only ever have one change in flight.
  CHECK(!current_change_);
  current_change_ = change;
}

void ConnectionManager::FinishChange() {
  // PrepareForChange/FinishChange should be balanced.
  CHECK(current_change_);
  current_change_ = NULL;
}

void ConnectionManager::DoAnimation() {
  if (!DecrementAnimatingViewsOpacity(GetRoot()))
    animation_timer_.Stop();
}

void ConnectionManager::AddConnection(ClientConnection* connection) {
  DCHECK_EQ(0u, connection_map_.count(connection->service()->id()));
  connection_map_[connection->service()->id()] = connection;
}

void ConnectionManager::PrepareToDestroyView(ServerView* view) {
  if (!in_destructor_ && IsViewAttachedToRoot(view) &&
      view->id() != ClonedViewId()) {
    // We're about to destroy a view. Any cloned views need to be reparented
    // else the animation would no longer be visible. By moving to a visible
    // view, view->parent(), we ensure the animation is still visible.
    ServerView* parent_above = view;
    ReparentClonedViews(view->parent(), &parent_above, view);
  }

  animation_runner_.CancelAnimationForView(view);
}

void ConnectionManager::PrepareToChangeViewHierarchy(ServerView* view,
                                                     ServerView* new_parent,
                                                     ServerView* old_parent) {
  if (view->id() == ClonedViewId() || in_destructor_)
    return;

  if (IsViewAttachedToRoot(view)) {
    // We're about to reparent a view. Any cloned views need to be reparented
    // else the animation may be effected in unusual ways. For example, the view
    // could move to a new location such that the animation is entirely clipped.
    // By moving to view->parent() we ensure the animation is still visible.
    ServerView* parent_above = view;
    ReparentClonedViews(view->parent(), &parent_above, view);
  }

  animation_runner_.CancelAnimationForView(view);
}

void ConnectionManager::PrepareToChangeViewVisibility(ServerView* view) {
  if (in_destructor_)
    return;

  if (IsViewAttachedToRoot(view) && view->id() != ClonedViewId() &&
      view->IsDrawn()) {
    // We're about to hide |view|, this would implicitly make any cloned views
    // hide too. Reparent so that animations are still visible.
    ServerView* parent_above = view;
    ReparentClonedViews(view->parent(), &parent_above, view);
  }

  const bool is_parent_drawn = view->parent() && view->parent()->IsDrawn();
  if (!is_parent_drawn || !view->visible())
    animation_runner_.CancelAnimationForView(view);
}

void ConnectionManager::OnScheduleViewPaint(const ServerView* view) {
  if (!in_destructor_)
    display_manager_->SchedulePaint(view, gfx::Rect(view->bounds().size()));
}

bool ConnectionManager::IsViewDrawn(const ServerView* view) const {
  // TODO(fsamuel): Iterate over all roots once we support multiple roots.
  if (!root_->visible())
    return false;
  while (view && view != root_.get() && view->visible())
    view = view->parent();
  return view == root_.get();
}

void ConnectionManager::OnViewDestroyed(ServerView* view) {
  if (!in_destructor_)
    ProcessViewDeleted(view->id());
}

void ConnectionManager::OnWillChangeViewHierarchy(ServerView* view,
                                                  ServerView* new_parent,
                                                  ServerView* old_parent) {
  if (view->id() == ClonedViewId() || in_destructor_)
    return;

  ProcessWillChangeViewHierarchy(view, new_parent, old_parent);
}

void ConnectionManager::OnViewHierarchyChanged(ServerView* view,
                                               ServerView* new_parent,
                                               ServerView* old_parent) {
  if (in_destructor_)
    return;

  ProcessViewHierarchyChanged(view, new_parent, old_parent);

  // TODO(beng): optimize.
  if (old_parent) {
    display_manager_->SchedulePaint(old_parent,
                                    gfx::Rect(old_parent->bounds().size()));
  }
  if (new_parent) {
    display_manager_->SchedulePaint(new_parent,
                                    gfx::Rect(new_parent->bounds().size()));
  }
}

void ConnectionManager::OnViewBoundsChanged(ServerView* view,
                                            const gfx::Rect& old_bounds,
                                            const gfx::Rect& new_bounds) {
  if (in_destructor_)
    return;

  ProcessViewBoundsChanged(view, old_bounds, new_bounds);
  if (!view->parent())
    return;

  // TODO(sky): optimize this.
  display_manager_->SchedulePaint(view->parent(), old_bounds);
  display_manager_->SchedulePaint(view->parent(), new_bounds);
}

void ConnectionManager::OnViewReordered(ServerView* view,
                                        ServerView* relative,
                                        mojo::OrderDirection direction) {
  if (!in_destructor_)
    display_manager_->SchedulePaint(view, gfx::Rect(view->bounds().size()));
}

void ConnectionManager::OnWillChangeViewVisibility(ServerView* view) {
  if (in_destructor_)
    return;

  // Need to repaint if the view was drawn (which means it's in the process of
  // hiding) or the view is transitioning to drawn.
  if (view->parent() && (view->IsDrawn() ||
      (!view->visible() && view->parent()->IsDrawn()))) {
    display_manager_->SchedulePaint(view->parent(), view->bounds());
  }

  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWillChangeViewVisibility(
        view, IsChangeSource(pair.first));
  }
}

void ConnectionManager::OnViewSharedPropertyChanged(
    ServerView* view,
    const std::string& name,
    const std::vector<uint8_t>* new_data) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewPropertyChanged(
        view, name, new_data, IsChangeSource(pair.first));
  }
}

void ConnectionManager::SetViewManagerRootClient(
    mojo::ViewManagerRootClientPtr client) {
  view_manager_root_client_ = client.Pass();
}

void ConnectionManager::SetViewportSize(mojo::SizePtr size) {
  display_manager_->SetViewportSize(size.To<gfx::Size>());
}

void ConnectionManager::CloneAndAnimate(mojo::Id transport_view_id) {
  CloneAndAnimate(ViewIdFromTransportId(transport_view_id));
}

void ConnectionManager::AddAccelerator(mojo::KeyboardCode keyboard_code,
                                       mojo::EventFlags flags) {
  event_dispatcher_.AddAccelerator(keyboard_code, flags);
}

void ConnectionManager::RemoveAccelerator(mojo::KeyboardCode keyboard_code,
                                          mojo::EventFlags flags) {
  event_dispatcher_.RemoveAccelerator(keyboard_code, flags);
}

void ConnectionManager::OnFocusChanged(ServerView* old_focused_view,
                                       ServerView* new_focused_view) {
  // There are up to four connections that need to be notified:
  // . the connection containing |old_focused_view|.
  // . the connection with |old_focused_view| as its root.
  // . the connection containing |new_focused_view|.
  // . the connection with |new_focused_view| as its root.
  // Some of these connections may be the same. The following takes care to
  // notify each only once.
  ViewManagerServiceImpl* owning_connection_old = nullptr;
  ViewManagerServiceImpl* embedded_connection_old = nullptr;

  if (old_focused_view) {
    owning_connection_old = GetConnection(old_focused_view->id().connection_id);
    if (owning_connection_old) {
      owning_connection_old->ProcessFocusChanged(old_focused_view,
                                                 new_focused_view);
    }
    embedded_connection_old = GetConnectionWithRoot(old_focused_view->id());
    if (embedded_connection_old) {
      DCHECK_NE(owning_connection_old, embedded_connection_old);
      embedded_connection_old->ProcessFocusChanged(old_focused_view,
                                                   new_focused_view);
    }
  }
  ViewManagerServiceImpl* owning_connection_new = nullptr;
  ViewManagerServiceImpl* embedded_connection_new = nullptr;
  if (new_focused_view) {
    owning_connection_new = GetConnection(new_focused_view->id().connection_id);
    if (owning_connection_new &&
        owning_connection_new != owning_connection_old &&
        owning_connection_new != embedded_connection_old) {
      owning_connection_new->ProcessFocusChanged(old_focused_view,
                                                 new_focused_view);
    }
    embedded_connection_new = GetConnectionWithRoot(new_focused_view->id());
    if (embedded_connection_new &&
        embedded_connection_new != owning_connection_old &&
        embedded_connection_new != embedded_connection_old) {
      DCHECK_NE(owning_connection_new, embedded_connection_new);
      embedded_connection_new->ProcessFocusChanged(old_focused_view,
                                                   new_focused_view);
    }
  }

  if (has_window_manager_client_connection()) {
    // Window manager should always be notified of focus change.
    ViewManagerServiceImpl* wm_connection =
        window_manager_client_connection_->service();
    if (wm_connection != owning_connection_old &&
        wm_connection != embedded_connection_old &&
        wm_connection != owning_connection_new &&
        wm_connection != embedded_connection_new) {
      wm_connection->ProcessFocusChanged(old_focused_view, new_focused_view);
    }
  }
}

}  // namespace view_manager
