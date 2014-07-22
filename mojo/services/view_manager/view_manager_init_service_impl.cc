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

void ViewManagerInitServiceImpl::MaybeEmbedRoot(
    const std::string& url,
    const Callback<void(bool)>& callback) {
  if (!is_tree_host_ready_)
    return;

  root_node_manager_.EmbedRoot(url);
  callback.Run(true);
}

void ViewManagerInitServiceImpl::EmbedRoot(
    const String& url,
    const Callback<void(bool)>& callback) {
  // TODO(beng): This means you can only have one EmbedRoot in flight at a time.
  //             Keep a vector of these around instead.
  connect_params_.reset(new ConnectParams);
  connect_params_->url = url.To<std::string>();
  connect_params_->callback = callback;
  MaybeEmbedRoot(url.To<std::string>(), callback);
}

void ViewManagerInitServiceImpl::OnRootViewManagerWindowTreeHostCreated() {
  DCHECK(!is_tree_host_ready_);
  is_tree_host_ready_ = true;
  if (connect_params_)
    MaybeEmbedRoot(connect_params_->url, connect_params_->callback);
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
