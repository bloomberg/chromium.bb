// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/application_runner_chromium.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/html_viewer/blink_platform_impl.h"
#include "mojo/services/html_viewer/html_document_view.h"
#include "mojo/services/public/interfaces/content_handler/content_handler.mojom.h"
#include "third_party/WebKit/public/web/WebKit.h"

namespace mojo {

class HTMLViewer;

class ContentHandlerImpl : public InterfaceImpl<ContentHandler> {
 public:
  explicit ContentHandlerImpl(Shell* shell) : shell_(shell) {}
  virtual ~ContentHandlerImpl() {}

 private:
  // Overridden from ContentHandler:
  virtual void OnConnect(
      const mojo::String& url,
      URLResponsePtr response,
      InterfaceRequest<ServiceProvider> service_provider_request) OVERRIDE {
    new HTMLDocumentView(
        response.Pass(), service_provider_request.Pass(), shell_);
  }

  Shell* shell_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

class HTMLViewer : public ApplicationDelegate {
 public:
  HTMLViewer() {}

  virtual ~HTMLViewer() { blink::shutdown(); }

 private:
  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) OVERRIDE {
    content_handler_factory_.reset(
        new InterfaceFactoryImplWithContext<ContentHandlerImpl, Shell>(
            app->shell()));
    blink_platform_impl_.reset(new BlinkPlatformImpl(app));
    blink::initialize(blink_platform_impl_.get());
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      OVERRIDE {
    connection->AddService(content_handler_factory_.get());
    return true;
  }

  scoped_ptr<BlinkPlatformImpl> blink_platform_impl_;
  scoped_ptr<InterfaceFactoryImplWithContext<ContentHandlerImpl, Shell> >
      content_handler_factory_;

  DISALLOW_COPY_AND_ASSIGN(HTMLViewer);
};

}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::HTMLViewer);
  return runner.Run(shell_handle);
}
