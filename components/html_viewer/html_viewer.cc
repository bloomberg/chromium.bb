// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_viewer.h"

#include <utility>

#include "components/html_viewer/content_handler_impl.h"
#include "components/html_viewer/global_state.h"
#include "mojo/shell/public/cpp/application_connection.h"

namespace html_viewer {

HTMLViewer::HTMLViewer() : app_(nullptr) {
}

HTMLViewer::~HTMLViewer() {
}

void HTMLViewer::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;
  global_state_.reset(new GlobalState(app));
}

bool HTMLViewer::AcceptConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService(this);
  return true;
}

void HTMLViewer::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::ContentHandler> request) {
  new ContentHandlerImpl(global_state_.get(), app_, std::move(request));
}

}  // namespace html_viewer
