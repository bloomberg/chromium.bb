// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/content_handler_impl.h"

#include <utility>

#include "components/html_viewer/html_document_application_delegate.h"

namespace html_viewer {

ContentHandlerImpl::ContentHandlerImpl(
    GlobalState* global_state,
    mojo::Shell* shell,
    mojo::InterfaceRequest<mojo::shell::mojom::ContentHandler> request)
    : global_state_(global_state),
      shell_(shell),
      binding_(this, std::move(request)),
      app_refcount_(shell_->CreateAppRefCount()) {}

ContentHandlerImpl::~ContentHandlerImpl() {
}

void ContentHandlerImpl::StartApplication(
    mojo::ApplicationRequest request,
    mojo::URLResponsePtr response,
    const mojo::Callback<void()>& destruct_callback) {
  // HTMLDocumentApplicationDelegate deletes itself.
  new HTMLDocumentApplicationDelegate(
      std::move(request), std::move(response), global_state_,
      shell_->CreateAppRefCount(), destruct_callback);
}

}  // namespace html_viewer
