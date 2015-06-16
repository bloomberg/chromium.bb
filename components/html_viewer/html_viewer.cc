// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util.h"
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
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
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

// ApplicationDelegate created by the content handler for a specific url.
class HTMLDocumentApplicationDelegate : public mojo::ApplicationDelegate {
 public:
  HTMLDocumentApplicationDelegate(
      mojo::InterfaceRequest<mojo::Application> request,
      mojo::URLResponsePtr response,
      Setup* setup,
      scoped_ptr<mojo::AppRefCount> parent_app_refcount)
      : app_(this,
             request.Pass(),
             base::Bind(&HTMLDocumentApplicationDelegate::OnTerminate,
                        base::Unretained(this))),
        parent_app_refcount_(parent_app_refcount.Pass()),
        url_(response->url),
        initial_response_(response.Pass()),
        setup_(setup) {}

 private:
  ~HTMLDocumentApplicationDelegate() override {
    // Deleting the documents is going to trigger a callback to
    // OnHTMLDocumentDeleted() and remove from |documents_|. Copy the set so we
    // don't have to worry about the set being modified out from under us.
    std::set<HTMLDocument*> documents(documents_);
    for (HTMLDocument* doc : documents)
      doc->Destroy();
  }

  // Callback from the quit closure. We key off this rather than
  // ApplicationDelegate::Quit() as we don't want to shut down the messageloop
  // when we quit (the messageloop is shared among multiple
  // HTMLDocumentApplicationDelegates).
  void OnTerminate() {
    delete this;
  }

  // ApplicationDelegate;
  void Initialize(mojo::ApplicationImpl* app) override {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:network_service");
    mojo::ApplicationConnection* connection =
        app_.ConnectToApplication(request.Pass());
    connection->ConnectToService(&network_service_);
    connection->ConnectToService(&url_loader_factory_);
  }
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    if (initial_response_) {
      OnResponseReceived(URLLoaderPtr(), connection, initial_response_.Pass());
    } else {
      URLLoaderPtr loader;
      url_loader_factory_->CreateURLLoader(GetProxy(&loader));
      mojo::URLRequestPtr request(mojo::URLRequest::New());
      request->url = url_;
      request->auto_follow_redirects = true;

      // |loader| will be passed to the OnResponseReceived method through a
      // callback. Because order of evaluation is undefined, a reference to the
      // raw pointer is needed.
      mojo::URLLoader* raw_loader = loader.get();
      raw_loader->Start(
          request.Pass(),
          base::Bind(&HTMLDocumentApplicationDelegate::OnResponseReceived,
                     base::Unretained(this), base::Passed(&loader),
                     connection));
    }
    return true;
  }

  void OnHTMLDocumentDeleted(HTMLDocument* document) {
    DCHECK(documents_.count(document) > 0);
    documents_.erase(document);
  }

  void OnResponseReceived(URLLoaderPtr loader,
                          mojo::ApplicationConnection* connection,
                          URLResponsePtr response) {
    // HTMLDocument is destroyed when the hosting view is destroyed, or
    // explicitly from our destructor.
    HTMLDocument* document = new HTMLDocument(
        &app_, connection, response.Pass(), setup_,
        base::Bind(&HTMLDocumentApplicationDelegate::OnHTMLDocumentDeleted,
                   base::Unretained(this)));
    documents_.insert(document);
  }

  mojo::ApplicationImpl app_;
  // AppRefCount of the parent (HTMLViewer).
  scoped_ptr<mojo::AppRefCount> parent_app_refcount_;
  const String url_;
  mojo::NetworkServicePtr network_service_;
  mojo::URLLoaderFactoryPtr url_loader_factory_;
  URLResponsePtr initial_response_;
  Setup* setup_;

  // As we create HTMLDocuments they are added here. They are removed when the
  // HTMLDocument is deleted.
  std::set<HTMLDocument*> documents_;

  DISALLOW_COPY_AND_ASSIGN(HTMLDocumentApplicationDelegate);
};

class ContentHandlerImpl : public mojo::ContentHandler {
 public:
  ContentHandlerImpl(Setup* setup,
                     mojo::ApplicationImpl* app,
                     mojo::InterfaceRequest<ContentHandler> request)
      : setup_(setup), app_(app), binding_(this, request.Pass()) {}
  ~ContentHandlerImpl() override {}

 private:
  // Overridden from ContentHandler:
  void StartApplication(InterfaceRequest<mojo::Application> request,
                        URLResponsePtr response) override {
    // HTMLDocumentApplicationDelegate deletes itself.
    new HTMLDocumentApplicationDelegate(
        request.Pass(), response.Pass(), setup_,
        app_->app_lifetime_helper()->CreateAppRefCount());
  }

  Setup* setup_;
  mojo::ApplicationImpl* app_;
  mojo::StrongBinding<mojo::ContentHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

class HTMLViewer : public mojo::ApplicationDelegate,
                   public mojo::InterfaceFactory<ContentHandler> {
 public:
  HTMLViewer() : app_(nullptr) {}
  ~HTMLViewer() override {}

 private:
  // Overridden from ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override {
    app_ = app;
    setup_.reset(new Setup(app));
  }

  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService(this);
    return true;
  }

  // Overridden from InterfaceFactory<ContentHandler>
  void Create(ApplicationConnection* connection,
              mojo::InterfaceRequest<ContentHandler> request) override {
    new ContentHandlerImpl(setup_.get(), app_, request.Pass());
  }

  scoped_ptr<Setup> setup_;
  mojo::ApplicationImpl* app_;

  DISALLOW_COPY_AND_ASSIGN(HTMLViewer);
};

}  // namespace html_viewer

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new html_viewer::HTMLViewer);
  return runner.Run(shell_handle);
}
