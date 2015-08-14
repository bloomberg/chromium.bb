// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/layout_test_content_handler_impl.h"

#include "base/bind.h"
#include "components/html_viewer/html_document_application_delegate.h"
#include "components/html_viewer/web_test_delegate_impl.h"
#include "components/test_runner/web_frame_test_proxy.h"
#include "components/test_runner/web_test_proxy.h"
#include "third_party/WebKit/public/web/WebTestingSupport.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace html_viewer {

LayoutTestContentHandlerImpl::LayoutTestContentHandlerImpl(
    GlobalState* global_state,
    mojo::ApplicationImpl* app,
    mojo::InterfaceRequest<mojo::ContentHandler> request,
    test_runner::WebTestInterfaces* test_interfaces,
    WebTestDelegateImpl* test_delegate)
    : ContentHandlerImpl(global_state, app, request.Pass()),
      test_interfaces_(test_interfaces),
      test_delegate_(test_delegate) {
}

LayoutTestContentHandlerImpl::~LayoutTestContentHandlerImpl() {
}

void LayoutTestContentHandlerImpl::StartApplication(
    mojo::InterfaceRequest<mojo::Application> request,
    mojo::URLResponsePtr response) {
  test_interfaces_->SetTestIsRunning(true);
  test_interfaces_->ConfigureForTestWithURL(GURL(), false);

  // HTMLDocumentApplicationDelegate deletes itself.
  HTMLDocumentApplicationDelegate* delegate =
      new HTMLDocumentApplicationDelegate(
          request.Pass(), response.Pass(), global_state(),
          app()->app_lifetime_helper()->CreateAppRefCount());

  delegate->SetHTMLFrameCreationCallback(base::Bind(
      &LayoutTestContentHandlerImpl::CreateHTMLFrame, base::Unretained(this)));
}

HTMLFrame* LayoutTestContentHandlerImpl::CreateHTMLFrame(
    HTMLFrame::CreateParams* params) {
  using ProxyType = test_runner::WebTestProxy<
      test_runner::WebFrameTestProxy<HTMLFrame, HTMLFrame::CreateParams*>,
      HTMLFrame::CreateParams*>;
  // TODO(sky): this isn't right for all frame types, eg remote frames.
  ProxyType* proxy = new ProxyType(params);
  proxy->SetInterfaces(test_interfaces_);
  proxy->SetDelegate(test_delegate_);
  proxy->set_base_proxy(proxy);
  test_delegate_->set_test_proxy(proxy);
  test_interfaces_->SetWebView(proxy->web_view(), proxy);
  proxy->set_widget(proxy->web_view());
  blink::WebTestingSupport::injectInternalsObject(
      proxy->web_view()->mainFrame()->toWebLocalFrame());
  test_interfaces_->BindTo(proxy->web_view()->mainFrame());
  return proxy;
}

}  // namespace html_viewer
