// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_LAYOUT_TEST_CONTENT_HANDLER_IMPL_H_
#define COMPONENTS_HTML_VIEWER_LAYOUT_TEST_CONTENT_HANDLER_IMPL_H_

#include "components/html_viewer/content_handler_impl.h"
#include "components/html_viewer/html_frame.h"

namespace test_runner {
class WebTestInterfaces;
}

namespace html_viewer {

class WebTestDelegateImpl;

class LayoutTestContentHandlerImpl : public ContentHandlerImpl {
 public:
  LayoutTestContentHandlerImpl(GlobalState* global_state,
                               mojo::ApplicationImpl* app,
                               mojo::InterfaceRequest<ContentHandler> request,
                               test_runner::WebTestInterfaces* test_interfaces,
                               WebTestDelegateImpl* test_delegate);
  ~LayoutTestContentHandlerImpl() override;

 private:
  // Overridden from ContentHandler:
  void StartApplication(mojo::InterfaceRequest<mojo::Application> request,
                        mojo::URLResponsePtr response) override;

  HTMLFrame* CreateHTMLFrame(HTMLFrame::CreateParams* params);

  test_runner::WebTestInterfaces* test_interfaces_;
  WebTestDelegateImpl* test_delegate_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestContentHandlerImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_LAYOUT_TEST_CONTENT_HANDLER_IMPL_H_
