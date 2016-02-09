// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_document_application_delegate.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_document.h"
#include "mojo/shell/public/cpp/interface_binder.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace html_viewer {

// InterfaceBinderQueue records all incoming service requests and processes
// them once PushRequestsTo() is called. This is useful if you need to delay
// processing incoming service requests.
class HTMLDocumentApplicationDelegate::InterfaceBinderQueue
    : public mojo::InterfaceBinder {
 public:
  InterfaceBinderQueue() {}
  ~InterfaceBinderQueue() override {}

  void PushRequestsTo(mojo::Connection* connection) {
    ScopedVector<Request> requests;
    requests_.swap(requests);
    for (Request* request : requests) {
      connection->GetLocalServiceProvider()->ConnectToService(
          request->interface_name, std::move(request->handle));
    }
  }

 private:
  struct Request {
    std::string interface_name;
    mojo::ScopedMessagePipeHandle handle;
  };

  // mojo::InterfaceBinder:
  void BindInterface(mojo::Connection* connection,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle handle) override {
    scoped_ptr<Request> request(new Request);
    request->interface_name = interface_name;
    request->handle = std::move(handle);
    requests_.push_back(std::move(request));
  }

  ScopedVector<Request> requests_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceBinderQueue);
};

HTMLDocumentApplicationDelegate::HTMLDocumentApplicationDelegate(
    mojo::ShellClientRequest request,
    mojo::URLResponsePtr response,
    GlobalState* global_state,
    scoped_ptr<mojo::AppRefCount> parent_app_refcount,
    const mojo::Callback<void()>& destruct_callback)
    : app_(this,
           std::move(request),
           base::Bind(&HTMLDocumentApplicationDelegate::OnTerminate,
                      base::Unretained(this))),
      parent_app_refcount_(std::move(parent_app_refcount)),
      url_(response->url),
      initial_response_(std::move(response)),
      global_state_(global_state),
      html_factory_(this),
      destruct_callback_(destruct_callback),
      weak_factory_(this) {}

HTMLDocumentApplicationDelegate::~HTMLDocumentApplicationDelegate() {
  // Deleting the documents is going to trigger a callback to
  // OnHTMLDocumentDeleted() and remove from |documents_|. Copy the set so we
  // don't have to worry about the set being modified out from under us.
  std::set<HTMLDocument*> documents2(documents2_);
  for (HTMLDocument* doc : documents2)
    doc->Destroy();
  DCHECK(documents2_.empty());
  destruct_callback_.Run();
}

// Callback from the quit closure. We key off this rather than
// mojo::ShellClient::Quit() as we don't want to shut down the messageloop
// when we quit (the messageloop is shared among multiple
// HTMLDocumentApplicationDelegates).
void HTMLDocumentApplicationDelegate::OnTerminate() {
  delete this;
}

// mojo::ShellClient;
void HTMLDocumentApplicationDelegate::Initialize(mojo::Shell* shell,
                                                 const std::string& url,
                                                 uint32_t id) {
  app_.ConnectToService("mojo:network_service", &url_loader_factory_);
}

bool HTMLDocumentApplicationDelegate::AcceptConnection(
    mojo::Connection* connection) {
  if (initial_response_) {
    OnResponseReceived(nullptr, mojo::URLLoaderPtr(), connection, nullptr,
                       std::move(initial_response_));
  } else if (url_ == "about:blank") {
    // This is a little unfortunate. At the browser side, when starting a new
    // app for "about:blank", the application manager uses
    // mojo::shell::AboutFetcher to construct a response for "about:blank".
    // However, when an app for "about:blank" already exists, it is reused and
    // we end up here. We cannot fetch the URL using mojo::URLLoader because it
    // is not an actual Web resource.
    // TODO(yzshen): find out a better approach.
    mojo::URLResponsePtr response(mojo::URLResponse::New());
    response->url = url_;
    response->status_code = 200;
    response->mime_type = "text/html";
    OnResponseReceived(nullptr, mojo::URLLoaderPtr(), connection, nullptr,
                       std::move(response));
  } else {
    // HTMLDocument provides services, but is created asynchronously. Queue up
    // requests until the HTMLDocument is created.
    scoped_ptr<InterfaceBinderQueue> interface_binder_queue(
        new InterfaceBinderQueue);
    connection->SetDefaultInterfaceBinder(interface_binder_queue.get());

    mojo::URLLoaderPtr loader;
    url_loader_factory_->CreateURLLoader(GetProxy(&loader));
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = url_;
    request->auto_follow_redirects = true;

    // |loader| will be passed to the OnResponseReceived method through a
    // callback. Because order of evaluation is undefined, a reference to the
    // raw pointer is needed.
    mojo::URLLoader* raw_loader = loader.get();
    // The app needs to stay alive while waiting for the response to be
    // available.
    scoped_ptr<mojo::AppRefCount> app_retainer(app_.CreateAppRefCount());
    raw_loader->Start(
        std::move(request),
        base::Bind(&HTMLDocumentApplicationDelegate::OnResponseReceived,
                   weak_factory_.GetWeakPtr(), base::Passed(&app_retainer),
                   base::Passed(&loader), connection,
                   base::Passed(&interface_binder_queue)));
  }
  return true;
}

void HTMLDocumentApplicationDelegate::OnHTMLDocumentDeleted2(
    HTMLDocument* document) {
  DCHECK(documents2_.count(document) > 0);
  documents2_.erase(document);
}

void HTMLDocumentApplicationDelegate::OnResponseReceived(
    scoped_ptr<mojo::AppRefCount> app_refcount,
    mojo::URLLoaderPtr loader,
    mojo::Connection* connection,
    scoped_ptr<InterfaceBinderQueue> binder_queue,
    mojo::URLResponsePtr response) {
  // HTMLDocument is destroyed when the hosting view is destroyed, or
  // explicitly from our destructor.
  HTMLDocument* document = new HTMLDocument(
      &app_, connection, std::move(response), global_state_,
      base::Bind(&HTMLDocumentApplicationDelegate::OnHTMLDocumentDeleted2,
                 base::Unretained(this)),
      html_factory_);
  documents2_.insert(document);

  if (binder_queue) {
    binder_queue->PushRequestsTo(connection);
    connection->SetDefaultInterfaceBinder(nullptr);
  }
}

HTMLFrame* HTMLDocumentApplicationDelegate::CreateHTMLFrame(
    HTMLFrame::CreateParams* params) {
  return new HTMLFrame(params);
}

HTMLWidgetRootLocal* HTMLDocumentApplicationDelegate::CreateHTMLWidgetRootLocal(
    HTMLWidgetRootLocal::CreateParams* params) {
  return new HTMLWidgetRootLocal(params);
}

}  // namespace html_viewer
