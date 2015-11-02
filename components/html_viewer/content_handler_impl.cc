// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/content_handler_impl.h"

#include "components/html_viewer/html_document_application_delegate.h"

namespace html_viewer {

ContentHandlerImpl::ContentHandlerImpl(
    GlobalState* global_state,
    mojo::ApplicationImpl* app,
    mojo::InterfaceRequest<mojo::ContentHandler> request)
    : global_state_(global_state),
      app_(app),
      binding_(this, request.Pass()),
      app_refcount_(app_->app_lifetime_helper()->CreateAppRefCount()) {
}

ContentHandlerImpl::~ContentHandlerImpl() {
}

void ContentHandlerImpl::StartApplication(
    mojo::InterfaceRequest<mojo::Application> request,
    mojo::URLResponsePtr response,
    const mojo::Callback<void()>& destruct_callback) {
  // HTMLDocumentApplicationDelegate deletes itself.
  new HTMLDocumentApplicationDelegate(
      request.Pass(), response.Pass(), global_state_,
      app_->app_lifetime_helper()->CreateAppRefCount(), destruct_callback);
}

}  // namespace html_viewer
