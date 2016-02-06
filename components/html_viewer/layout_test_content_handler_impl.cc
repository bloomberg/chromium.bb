// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/layout_test_content_handler_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_document_application_delegate.h"
#include "components/html_viewer/html_widget.h"
#include "components/html_viewer/layout_test_blink_settings_impl.h"
#include "components/html_viewer/web_test_delegate_impl.h"
#include "components/test_runner/web_frame_test_proxy.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebTestingSupport.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace html_viewer {

class TestHTMLFrame : public HTMLFrame {
 public:
  explicit TestHTMLFrame(HTMLFrame::CreateParams* params)
      : HTMLFrame(params), test_interfaces_(nullptr) {}

  void set_test_interfaces(test_runner::WebTestInterfaces* test_interfaces) {
    test_interfaces_ = test_interfaces;
  }

 protected:
  ~TestHTMLFrame() override {}

 private:
  // blink::WebFrameClient::
  void didClearWindowObject(blink::WebLocalFrame* frame) override {
    HTMLFrame::didClearWindowObject(frame);
    blink::WebTestingSupport::injectInternalsObject(frame);
    DCHECK(test_interfaces_);
    test_interfaces_->BindTo(frame);
  }

  test_runner::WebTestInterfaces* test_interfaces_;

  DISALLOW_COPY_AND_ASSIGN(TestHTMLFrame);
};

LayoutTestContentHandlerImpl::LayoutTestContentHandlerImpl(
    GlobalState* global_state,
    mojo::ApplicationImpl* app,
    mojo::InterfaceRequest<mojo::shell::mojom::ContentHandler> request,
    test_runner::WebTestInterfaces* test_interfaces,
    WebTestDelegateImpl* test_delegate)
    : ContentHandlerImpl(global_state, app, std::move(request)),
      test_interfaces_(test_interfaces),
      test_delegate_(test_delegate),
      web_widget_proxy_(nullptr),
      app_refcount_(app->app_lifetime_helper()->CreateAppRefCount()) {}

LayoutTestContentHandlerImpl::~LayoutTestContentHandlerImpl() {
}

void LayoutTestContentHandlerImpl::StartApplication(
    mojo::ApplicationRequest request,
    mojo::URLResponsePtr response,
    const mojo::Callback<void()>& destruct_callback) {
  test_interfaces_->SetTestIsRunning(true);
  test_interfaces_->ConfigureForTestWithURL(GURL(), false);

  // HTMLDocumentApplicationDelegate deletes itself.
  HTMLDocumentApplicationDelegate* delegate =
      new HTMLDocumentApplicationDelegate(
          std::move(request), std::move(response), global_state(),
          app()->app_lifetime_helper()->CreateAppRefCount(), destruct_callback);

  delegate->set_html_factory(this);
}

HTMLWidgetRootLocal* LayoutTestContentHandlerImpl::CreateHTMLWidgetRootLocal(
    HTMLWidgetRootLocal::CreateParams* params) {
  web_widget_proxy_ = new WebWidgetProxy(params);
  return web_widget_proxy_;
}

HTMLFrame* LayoutTestContentHandlerImpl::CreateHTMLFrame(
    HTMLFrame::CreateParams* params) {
  params->manager->global_state()->set_blink_settings(
      new LayoutTestBlinkSettingsImpl());

  // The test harness isn't correctly set-up for iframes yet. So return a normal
  // HTMLFrame for iframes.
  if (params->parent || !params->window || params->window->id() != params->id)
    return new HTMLFrame(params);

  using ProxyType =
      test_runner::WebFrameTestProxy<TestHTMLFrame, HTMLFrame::CreateParams*>;
  ProxyType* proxy = new ProxyType(params);
  proxy->set_test_interfaces(test_interfaces_);

  web_widget_proxy_->SetInterfaces(test_interfaces_);
  web_widget_proxy_->SetDelegate(test_delegate_);
  proxy->set_base_proxy(web_widget_proxy_);
  test_delegate_->set_test_proxy(web_widget_proxy_);
  test_interfaces_->SetWebView(web_widget_proxy_->web_view(),
                               web_widget_proxy_);
  web_widget_proxy_->set_widget(web_widget_proxy_->web_view());
  test_interfaces_->BindTo(web_widget_proxy_->web_view()->mainFrame());
  return proxy;
}

}  // namespace html_viewer
