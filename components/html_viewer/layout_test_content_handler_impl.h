// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_LAYOUT_TEST_CONTENT_HANDLER_IMPL_H_
#define COMPONENTS_HTML_VIEWER_LAYOUT_TEST_CONTENT_HANDLER_IMPL_H_

#include "base/macros.h"
#include "components/html_viewer/content_handler_impl.h"
#include "components/html_viewer/html_factory.h"
#include "components/test_runner/web_test_proxy.h"

namespace blink {
class WebView;
}

namespace test_runner {
class WebTestInterfaces;
}

namespace html_viewer {

class WebTestDelegateImpl;

class LayoutTestContentHandlerImpl : public ContentHandlerImpl,
                                     public HTMLFactory {
 public:
  LayoutTestContentHandlerImpl(GlobalState* global_state,
                               mojo::Shell* shell,
                               mojo::InterfaceRequest<ContentHandler> request,
                               test_runner::WebTestInterfaces* test_interfaces,
                               WebTestDelegateImpl* test_delegate);
  ~LayoutTestContentHandlerImpl() override;

 private:
  using WebWidgetProxy =
      test_runner::WebTestProxy<HTMLWidgetRootLocal,
                                HTMLWidgetRootLocal::CreateParams*>;

  // ContentHandler:
  void StartApplication(
      mojo::ShellClientRequest request,
      mojo::URLResponsePtr response,
      const mojo::Callback<void()>& destruct_callback) override;

  // HTMLFactory
  HTMLFrame* CreateHTMLFrame(HTMLFrame::CreateParams* params) override;
  HTMLWidgetRootLocal* CreateHTMLWidgetRootLocal(
      HTMLWidgetRootLocal::CreateParams* params) override;

  test_runner::WebTestInterfaces* test_interfaces_;
  WebTestDelegateImpl* test_delegate_;
  WebWidgetProxy* web_widget_proxy_;
  scoped_ptr<mojo::AppRefCount> app_refcount_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestContentHandlerImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_LAYOUT_TEST_CONTENT_HANDLER_IMPL_H_
