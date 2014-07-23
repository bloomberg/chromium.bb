// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_init_service_impl.h"

#include "base/bind.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_service_impl.h"

namespace mojo {
class ApplicationConnection;
namespace view_manager {
namespace service {

ViewManagerInitServiceImpl::ConnectParams::ConnectParams() {}

ViewManagerInitServiceImpl::ConnectParams::~ConnectParams() {}

ViewManagerInitServiceImpl::ViewManagerInitServiceImpl(
    ApplicationConnection* connection)
    : root_node_manager_(
          connection,
          this,
          base::Bind(&ViewManagerInitServiceImpl::OnNativeViewportDeleted,
                     base::Unretained(this))),
      is_tree_host_ready_(false) {
}

ViewManagerInitServiceImpl::~ViewManagerInitServiceImpl() {
}

void ViewManagerInitServiceImpl::MaybeEmbed() {
  if (!is_tree_host_ready_)
    return;

  ScopedVector<ConnectParams>::const_iterator it = connect_params_.begin();
  for (; it != connect_params_.end(); ++it) {
    root_node_manager_.EmbedRoot((*it)->url);
    (*it)->callback.Run(true);
  }
  connect_params_.clear();
}

void ViewManagerInitServiceImpl::Embed(
    const String& url,
    const Callback<void(bool)>& callback) {
  ConnectParams* params = new ConnectParams;
  params->url = url.To<std::string>();
  params->callback = callback;
  connect_params_.push_back(params);
  MaybeEmbed();
}

void ViewManagerInitServiceImpl::OnRootViewManagerWindowTreeHostCreated() {
  DCHECK(!is_tree_host_ready_);
  is_tree_host_ready_ = true;
  MaybeEmbed();
}

void ViewManagerInitServiceImpl::OnNativeViewportDeleted() {
  // TODO(beng): Should not have to rely on implementation detail of
  //             InterfaceImpl to close the connection. Instead should simply
  //             be able to delete this object.
  internal_state()->router()->CloseMessagePipe();
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
