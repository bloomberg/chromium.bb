// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_init_service_impl.h"

#include "base/bind.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_init_service_context.h"
#include "mojo/services/view_manager/view_manager_service_impl.h"

namespace mojo {
namespace service {

ViewManagerInitServiceImpl::ViewManagerInitServiceImpl(
    ApplicationConnection* connection,
    ViewManagerInitServiceContext* context)
    : context_(context) {
  context_->AddConnection(this);
}

ViewManagerInitServiceImpl::~ViewManagerInitServiceImpl() {
  context_->RemoveConnection(this);
}

void ViewManagerInitServiceImpl::Embed(
    const String& url,
    ServiceProviderPtr service_provider,
    const Callback<void(bool)>& callback) {
  context_->Embed(url, service_provider.Pass(), callback);
}

}  // namespace service
}  // namespace mojo
