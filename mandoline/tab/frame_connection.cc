// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/tab/frame_connection.h"

#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace mandoline {

FrameConnection::FrameConnection() : application_connection_(nullptr) {
}

FrameConnection::~FrameConnection() {
}

void FrameConnection::Init(mojo::ApplicationImpl* app,
                           mojo::URLRequestPtr request,
                           mojo::ViewManagerClientPtr* view_manage_client) {
  DCHECK(!application_connection_);
  application_connection_ = app->ConnectToApplication(request.Pass());
  application_connection_->ConnectToService(view_manage_client);
  application_connection_->ConnectToService(&frame_tree_client_);
  frame_tree_client_.set_connection_error_handler([]() {
    // TODO(sky): implement this.
    NOTIMPLEMENTED();
  });
}

}  // namespace mandoline
