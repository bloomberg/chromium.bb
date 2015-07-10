// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/layout_test_html_viewer.h"

#include "components/html_viewer/global_state.h"
#include "components/html_viewer/layout_test_content_handler_impl.h"
#include "components/test_runner/web_test_interfaces.h"

namespace html_viewer {

LayoutTestHTMLViewer::LayoutTestHTMLViewer() {
}

LayoutTestHTMLViewer::~LayoutTestHTMLViewer() {
}

void LayoutTestHTMLViewer::Initialize(mojo::ApplicationImpl* app) {
  HTMLViewer::Initialize(app);
  global_state()->InitIfNecessary(gfx::Size(800, 600), 1.f);
  test_interfaces_.reset(new test_runner::WebTestInterfaces);
  test_interfaces_->ResetAll();
  test_delegate_.set_test_interfaces(test_interfaces_.get());
  test_interfaces_->SetDelegate(&test_delegate_);
}

void LayoutTestHTMLViewer::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::ContentHandler> request) {
  new LayoutTestContentHandlerImpl(global_state(), app(), request.Pass(),
                                   test_interfaces_.get(),
                                   &test_delegate_);
}

}  // namespace html_viewer
