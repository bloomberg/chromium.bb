// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_init_service_impl.h"

#include "base/bind.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_init_service_context.h"
#include "mojo/services/view_manager/view_manager_service_impl.h"

namespace mojo {
class ApplicationConnection;
namespace service {

ViewManagerInitServiceImpl::ConnectParams::ConnectParams() {}

ViewManagerInitServiceImpl::ConnectParams::~ConnectParams() {}

ViewManagerInitServiceImpl::ViewManagerInitServiceImpl(
    ApplicationConnection* connection,
    ViewManagerInitServiceContext* context)
    : context_(context) {
  context_->AddConnection(this);
}

ViewManagerInitServiceImpl::~ViewManagerInitServiceImpl() {
  context_->RemoveConnection(this);
}

void ViewManagerInitServiceImpl::OnNativeViewportDeleted() {
  delete this;
}

void ViewManagerInitServiceImpl::OnRootViewManagerWindowTreeHostCreated() {
  MaybeEmbed();
}

void ViewManagerInitServiceImpl::MaybeEmbed() {
  if (!context_->is_tree_host_ready())
    return;

  ScopedVector<ConnectParams>::const_iterator it = connect_params_.begin();
  for (; it != connect_params_.end(); ++it) {
    context_->root_node_manager()->EmbedRoot((*it)->url,
                                             (*it)->service_provider.Pass());
    (*it)->callback.Run(true);
  }
  connect_params_.clear();
}

void ViewManagerInitServiceImpl::Embed(
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

}  // namespace service
}  // namespace mojo
