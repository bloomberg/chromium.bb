// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_init_service_context.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "mojo/services/view_manager/connection_manager.h"
#include "mojo/services/view_manager/view_manager_init_service_impl.h"

namespace mojo {
namespace service {

ViewManagerInitServiceContext::ConnectParams::ConnectParams() {}

ViewManagerInitServiceContext::ConnectParams::~ConnectParams() {}

ViewManagerInitServiceContext::ViewManagerInitServiceContext()
    : deleting_connection_(false) {
}
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
  // message loop, so we need to destroy the ConnectionManager earlier, as it
  // may attempt to post tasks during its destruction.
  if (connections_.empty())
    connection_manager_.reset();
}

void ViewManagerInitServiceContext::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  if (!connection_manager_.get()) {
    connection_manager_.reset(new ConnectionManager(
        connection,
        base::Bind(&ViewManagerInitServiceContext::OnNativeViewportDeleted,
                   base::Unretained(this))));
  }
}

void ViewManagerInitServiceContext::Embed(
    const String& url,
    ServiceProviderPtr service_provider,
    const Callback<void(bool)>& callback) {
  connection_manager_->EmbedRoot(url, Get(&service_provider));
  callback.Run(true);
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
  connection_manager_.reset();
}

}  // namespace service
}  // namespace mojo
