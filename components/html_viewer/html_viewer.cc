// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_viewer.h"

#include <utility>

#include "components/html_viewer/content_handler_impl.h"
#include "components/html_viewer/global_state.h"
#include "mojo/shell/public/cpp/connection.h"

namespace html_viewer {

HTMLViewer::HTMLViewer() : shell_(nullptr) {
}

HTMLViewer::~HTMLViewer() {
}

void HTMLViewer::Initialize(mojo::Shell* shell, const std::string& url,
                            uint32_t id) {
  shell_ = shell;
  global_state_.reset(new GlobalState(shell, url));
}

bool HTMLViewer::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface(this);
  return true;
}

void HTMLViewer::Create(
    mojo::Connection* connection,
    mojo::InterfaceRequest<mojo::shell::mojom::ContentHandler> request) {
  new ContentHandlerImpl(global_state_.get(), shell_, std::move(request));
}

}  // namespace html_viewer
