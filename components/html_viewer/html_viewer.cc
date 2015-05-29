// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "components/html_viewer/html_document.h"
#include "components/html_viewer/setup.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

using mojo::ApplicationConnection;
using mojo::Array;
using mojo::BindToRequest;
using mojo::ContentHandler;
using mojo::InterfaceRequest;
using mojo::ServiceProvider;
using mojo::ServiceProviderPtr;
using mojo::ShellPtr;
using mojo::String;
using mojo::URLLoaderPtr;
using mojo::URLResponsePtr;

namespace html_viewer {

class HTMLViewer;

class HTMLViewerApplication : public mojo::Application {
 public:
  HTMLViewerApplication(InterfaceRequest<Application> request,
                        URLResponsePtr response,
                        Setup* setup)
      : app_refcount_(setup->app()->app_lifetime_helper()->CreateAppRefCount()),
        url_(response->url),
        binding_(this, request.Pass()),
        initial_response_(response.Pass()),
        setup_(setup) {
  }

  ~HTMLViewerApplication() override {
  }

  void Initialize(ShellPtr shell, const String& url) override {
    shell_ = shell.Pass();
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:network_service");
    setup_->app()->ConnectToService(request.Pass(), &network_service_);
  }

  void AcceptConnection(const String& requestor_url,
                        InterfaceRequest<ServiceProvider> services,
                        ServiceProviderPtr exposed_services,
                        const String& url) override {
    if (initial_response_) {
      OnResponseReceived(URLLoaderPtr(), services.Pass(),
                         initial_response_.Pass());
    } else {
      URLLoaderPtr loader;
      network_service_->CreateURLLoader(GetProxy(&loader));
      mojo::URLRequestPtr request(mojo::URLRequest::New());
      request->url = url_;
      request->auto_follow_redirects = true;

      // |loader| will be pass to the OnResponseReceived method through a
      // callback. Because order of evaluation is undefined, a reference to the
      // raw pointer is needed.
      mojo::URLLoader* raw_loader = loader.get();
      raw_loader->Start(
          request.Pass(),
          base::Bind(&HTMLViewerApplication::OnResponseReceived,
                     base::Unretained(this), base::Passed(&loader),
                     base::Passed(&services)));
    }
  }

  void OnQuitRequested(const mojo::Callback<void(bool)>& callback) override {
    callback.Run(true);
    delete this;
  }

 private:
  void OnResponseReceived(URLLoaderPtr loader,
                          InterfaceRequest<ServiceProvider> services,
                          URLResponsePtr response) {
    // HTMLDocument is destroyed when the hosting view is destroyed.
    // TODO(sky): when headless, this leaks.
    new HTMLDocument(services.Pass(), response.Pass(), shell_.Pass(), setup_);
  }

  scoped_ptr<mojo::AppRefCount> app_refcount_;
  String url_;
  mojo::StrongBinding<mojo::Application> binding_;
  ShellPtr shell_;
  mojo::NetworkServicePtr network_service_;
  URLResponsePtr initial_response_;
  Setup* setup_;

  DISALLOW_COPY_AND_ASSIGN(HTMLViewerApplication);
};

class ContentHandlerImpl : public mojo::ContentHandler {
 public:
  ContentHandlerImpl(Setup* setup,
                     mojo::InterfaceRequest<ContentHandler> request)
      : setup_(setup),
        binding_(this, request.Pass()) {}
  ~ContentHandlerImpl() override {}

 private:
  // Overridden from ContentHandler:
  void StartApplication(InterfaceRequest<mojo::Application> request,
                        URLResponsePtr response) override {
    // HTMLViewerApplication is owned by the binding.
    new HTMLViewerApplication(request.Pass(), response.Pass(), setup_);
  }

  Setup* setup_;
  mojo::StrongBinding<mojo::ContentHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

class HTMLViewer : public mojo::ApplicationDelegate,
                   public mojo::InterfaceFactory<ContentHandler> {
 public:
  HTMLViewer() {}
  ~HTMLViewer() override { blink::shutdown(); }

 private:
  // Overridden from ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override {
    setup_.reset(new Setup(app));
  }

  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    // If we're not being connected to from the view manager assume we're being
    // run in tests, or a headless environment, in which case we'll never get a
    // ui and there is no point in waiting for it.
    if (connection->GetRemoteApplicationURL() != "mojo://view_manager/" &&
        !setup_->did_init()) {
      setup_->InitHeadless();
    }
    connection->AddService(this);
    return true;
  }

  // Overridden from InterfaceFactory<ContentHandler>
  void Create(ApplicationConnection* connection,
              mojo::InterfaceRequest<ContentHandler> request) override {
    new ContentHandlerImpl(setup_.get(), request.Pass());
  }

  scoped_ptr<Setup> setup_;

  DISALLOW_COPY_AND_ASSIGN(HTMLViewer);
};

}  // namespace html_viewer

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new html_viewer::HTMLViewer);
  return runner.Run(shell_handle);
}
