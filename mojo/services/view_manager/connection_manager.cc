// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/connection_manager.h"

#include "base/logging.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"
#include "mojo/services/view_manager/view_manager_service_impl.h"

namespace mojo {
namespace service {

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
    ApplicationConnection* app_connection,
    const Callback<void()>& native_viewport_closed_callback)
    : app_connection_(app_connection),
      next_connection_id_(1),
      display_manager_(app_connection,
                       this,
                       native_viewport_closed_callback),
      root_(new ServerView(this, RootViewId())),
      current_change_(NULL) {
  root_->SetBounds(gfx::Rect(800, 600));
}

ConnectionManager::~ConnectionManager() {
  while (!connections_created_by_connect_.empty())
    delete *(connections_created_by_connect_.begin());
  // All the connections should have been destroyed.
  DCHECK(connection_map_.empty());
  root_.reset();
}

ConnectionSpecificId ConnectionManager::GetAndAdvanceNextConnectionId() {
  const ConnectionSpecificId id = next_connection_id_++;
  DCHECK_LT(id, next_connection_id_);
  return id;
}

void ConnectionManager::AddConnection(ViewManagerServiceImpl* connection) {
  DCHECK_EQ(0u, connection_map_.count(connection->id()));
  connection_map_[connection->id()] = connection;
}

void ConnectionManager::RemoveConnection(ViewManagerServiceImpl* connection) {
  connection_map_.erase(connection->id());
  connections_created_by_connect_.erase(connection);

  // Notify remaining connections so that they can cleanup.
  for (ConnectionMap::const_iterator i = connection_map_.begin();
       i != connection_map_.end();
       ++i) {
    i->second->OnViewManagerServiceImplDestroyed(connection->id());
  }
}

void ConnectionManager::EmbedRoot(
    const std::string& url,
    InterfaceRequest<ServiceProvider> service_provider) {
  if (connection_map_.empty()) {
    EmbedImpl(kInvalidConnectionId,
              String::From(url),
              RootViewId(),
              service_provider.Pass());
    return;
  }
  ViewManagerServiceImpl* connection = GetConnection(kWindowManagerConnection);
  connection->client()->Embed(url, service_provider.Pass());
}

void ConnectionManager::Embed(
    ConnectionSpecificId creator_id,
    const String& url,
    Id transport_view_id,
    InterfaceRequest<ServiceProvider> service_provider) {
  EmbedImpl(creator_id,
            url,
            ViewIdFromTransportId(transport_view_id),
            service_provider.Pass())->set_delete_on_connection_error();
}

ViewManagerServiceImpl* ConnectionManager::GetConnection(
    ConnectionSpecificId connection_id) {
  ConnectionMap::iterator i = connection_map_.find(connection_id);
  return i == connection_map_.end() ? NULL : i->second;
}

ServerView* ConnectionManager::GetView(const ViewId& id) {
  if (id == root_->id())
    return root_.get();
  ConnectionMap::iterator i = connection_map_.find(id.connection_id);
  return i == connection_map_.end() ? NULL : i->second->GetView(id);
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
  for (ConnectionMap::const_iterator i = connection_map_.begin();
       i != connection_map_.end();
       ++i) {
    if (i->second->HasRoot(id))
      return i->second;
  }
  return NULL;
}

void ConnectionManager::DispatchViewInputEventToWindowManager(EventPtr event) {
  // Input events are forwarded to the WindowManager. The WindowManager
  // eventually calls back to us with DispatchOnViewInputEvent().
  ViewManagerServiceImpl* connection = GetConnection(kWindowManagerConnection);
  if (!connection)
    return;
  connection->client()->DispatchOnViewInputEvent(event.Pass());
}

void ConnectionManager::ProcessViewBoundsChanged(const ServerView* view,
                                                 const gfx::Rect& old_bounds,
                                                 const gfx::Rect& new_bounds) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end();
       ++i) {
    i->second->ProcessViewBoundsChanged(
        view, old_bounds, new_bounds, IsChangeSource(i->first));
  }
}

void ConnectionManager::ProcessWillChangeViewHierarchy(
    const ServerView* view,
    const ServerView* new_parent,
    const ServerView* old_parent) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end();
       ++i) {
    i->second->ProcessWillChangeViewHierarchy(
        view, new_parent, old_parent, IsChangeSource(i->first));
  }
}

void ConnectionManager::ProcessViewHierarchyChanged(
    const ServerView* view,
    const ServerView* new_parent,
    const ServerView* old_parent) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end();
       ++i) {
    i->second->ProcessViewHierarchyChanged(
        view, new_parent, old_parent, IsChangeSource(i->first));
  }
}

void ConnectionManager::ProcessViewReorder(const ServerView* view,
                                           const ServerView* relative_view,
                                           const OrderDirection direction) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end();
       ++i) {
    i->second->ProcessViewReorder(
        view, relative_view, direction, IsChangeSource(i->first));
  }
}

void ConnectionManager::ProcessViewDeleted(const ViewId& view) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end();
       ++i) {
    i->second->ProcessViewDeleted(view, IsChangeSource(i->first));
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

ViewManagerServiceImpl* ConnectionManager::EmbedImpl(
    const ConnectionSpecificId creator_id,
    const String& url,
    const ViewId& root_id,
    InterfaceRequest<ServiceProvider> service_provider) {
  MessagePipe pipe;

  ServiceProvider* view_manager_service_provider =
      app_connection_->ConnectToApplication(url)->GetServiceProvider();
  view_manager_service_provider->ConnectToService(
      ViewManagerServiceImpl::Client::Name_, pipe.handle1.Pass());

  std::string creator_url;
  ConnectionMap::const_iterator it = connection_map_.find(creator_id);
  if (it != connection_map_.end())
    creator_url = it->second->url();

  ViewManagerServiceImpl* connection =
      new ViewManagerServiceImpl(this,
                                 creator_id,
                                 creator_url,
                                 url.To<std::string>(),
                                 root_id,
                                 service_provider.Pass());
  WeakBindToPipe(connection, pipe.handle0.Pass());
  connections_created_by_connect_.insert(connection);
  OnConnectionMessagedClient(connection->id());
  return connection;
}

void ConnectionManager::OnViewDestroyed(const ServerView* view) {
  ProcessViewDeleted(view->id());
}

void ConnectionManager::OnWillChangeViewHierarchy(
    const ServerView* view,
    const ServerView* new_parent,
    const ServerView* old_parent) {
  if (!display_manager_.in_setup())
    ProcessWillChangeViewHierarchy(view, new_parent, old_parent);
}

void ConnectionManager::OnViewHierarchyChanged(const ServerView* view,
                                               const ServerView* new_parent,
                                               const ServerView* old_parent) {
  if (!display_manager_.in_setup())
    ProcessViewHierarchyChanged(view, new_parent, old_parent);
  // TODO(beng): optimize.
  if (old_parent) {
    display_manager_.SchedulePaint(old_parent,
                                   gfx::Rect(old_parent->bounds().size()));
  }
  if (new_parent) {
    display_manager_.SchedulePaint(new_parent,
                                   gfx::Rect(new_parent->bounds().size()));
  }
}

void ConnectionManager::OnViewBoundsChanged(const ServerView* view,
                                            const gfx::Rect& old_bounds,
                                            const gfx::Rect& new_bounds) {
  ProcessViewBoundsChanged(view, old_bounds, new_bounds);
  if (!view->parent())
    return;

  // TODO(sky): optimize this.
  display_manager_.SchedulePaint(view->parent(), old_bounds);
  display_manager_.SchedulePaint(view->parent(), new_bounds);
}

void ConnectionManager::OnViewSurfaceIdChanged(const ServerView* view) {
  display_manager_.SchedulePaint(view, gfx::Rect(view->bounds().size()));
}

void ConnectionManager::OnWillChangeViewVisibility(const ServerView* view) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end();
       ++i) {
    i->second->ProcessWillChangeViewVisibility(view, IsChangeSource(i->first));
  }
}

}  // namespace service
}  // namespace mojo
