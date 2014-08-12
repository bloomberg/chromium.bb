// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_init_service_context.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/view_manager_init_service_impl.h"

namespace mojo {
namespace service {

ViewManagerInitServiceContext::ConnectParams::ConnectParams() {}

ViewManagerInitServiceContext::ConnectParams::~ConnectParams() {}

ViewManagerInitServiceContext::ViewManagerInitServiceContext()
    : is_tree_host_ready_(false),
      deleting_connection_(false) {}
ViewManagerInitServiceContext::~ViewManagerInitServiceContext() {}

void ViewManagerInitServiceContext::AddConnection(
    ViewManagerInitServiceImpl* connection) {
  DCHECK(std::find(connections_.begin(), connections_.end(), connection) ==
                   connections_.end());
  connections_.push_back(connection);
}

void ViewManagerInitServiceContext::RemoveConnection(
    ViewManagerInitServiceImpl* connection) {
  if (!deleting_connection_) {
    Connections::iterator it =
        std::find(connections_.begin(), connections_.end(), connection);
    DCHECK(it != connections_.end());
    connections_.erase(it);
  }

  // This object is owned by an object that outlives the current thread's
  // message loop, so we need to destroy the RootNodeManager earlier, as it may
  // attempt to post tasks during its destruction.
  if (connections_.empty())
    root_node_manager_.reset();
}

void ViewManagerInitServiceContext::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  if (!root_node_manager_.get()) {
    root_node_manager_.reset(new RootNodeManager(
        connection,
        this,
        base::Bind(&ViewManagerInitServiceContext::OnNativeViewportDeleted,
                   base::Unretained(this))));
  }
}

void ViewManagerInitServiceContext::Embed(
    const String& url,
    ServiceProviderPtr service_provider,
    const Callback<void(bool)>& callback) {
  ConnectParams* params = new ConnectParams;
  params->url = url.To<std::string>();
  params->callback = callback;
  params->service_provider.Bind(service_provider.PassMessagePipe());
  connect_params_.push_back(params);
  MaybeEmbed();
}

void ViewManagerInitServiceContext::OnRootViewManagerWindowTreeHostCreated() {
  DCHECK(!is_tree_host_ready_);
  is_tree_host_ready_ = true;
  MaybeEmbed();
}

void ViewManagerInitServiceContext::OnNativeViewportDeleted() {
  // Prevent the connection from modifying the connection list during manual
  // teardown.
  base::AutoReset<bool> deleting_connection(&deleting_connection_, true);
  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    delete *it;
  }
  connections_.clear();
  root_node_manager_.reset();
}

void ViewManagerInitServiceContext::MaybeEmbed() {
  if (!is_tree_host_ready_)
    return;

  for (ScopedVector<ConnectParams>::const_iterator it = connect_params_.begin();
       it != connect_params_.end(); ++it) {
    root_node_manager_->EmbedRoot((*it)->url, (*it)->service_provider.Pass());
    (*it)->callback.Run(true);
  }
  connect_params_.clear();
}

}  // namespace service
}  // namespace mojo
