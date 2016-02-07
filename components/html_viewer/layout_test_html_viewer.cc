// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/layout_test_html_viewer.h"

#include <utility>

#include "components/html_viewer/global_state.h"
#include "components/html_viewer/layout_test_content_handler_impl.h"
#include "components/test_runner/web_test_interfaces.h"
#include "components/web_view/test_runner/public/interfaces/layout_test_runner.mojom.h"
#include "v8/include/v8.h"

namespace html_viewer {

LayoutTestHTMLViewer::LayoutTestHTMLViewer() {
}

LayoutTestHTMLViewer::~LayoutTestHTMLViewer() {
}

void LayoutTestHTMLViewer::Initialize(mojo::Shell* shell,
                                      const std::string& url, uint32_t id) {
  HTMLViewer::Initialize(shell, url, id);
  global_state()->InitIfNecessary(gfx::Size(800, 600), 1.f);
  test_interfaces_.reset(new test_runner::WebTestInterfaces);
  test_interfaces_->ResetAll();
  test_delegate_.set_test_interfaces(test_interfaces_.get());
  test_delegate_.set_completion_callback(
      base::Bind(&LayoutTestHTMLViewer::TestFinished, base::Unretained(this)));
  test_interfaces_->SetDelegate(&test_delegate_);

  // Always expose GC to layout tests.
  std::string flags("--expose-gc");
  v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
}

void LayoutTestHTMLViewer::TestFinished() {
  test_interfaces_->ResetAll();

  web_view::LayoutTestRunnerPtr test_runner_ptr;
  shell()->ConnectToService("mojo:web_view_test_runner", &test_runner_ptr);
  test_runner_ptr->TestFinished();
}

void LayoutTestHTMLViewer::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::shell::mojom::ContentHandler> request) {
  new LayoutTestContentHandlerImpl(global_state(), shell(), std::move(request),
                                   test_interfaces_.get(), &test_delegate_);
}

}  // namespace html_viewer
