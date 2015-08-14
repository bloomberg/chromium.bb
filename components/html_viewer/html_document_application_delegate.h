// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_APPLICATION_DELEGATE_H_
#define COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_APPLICATION_DELEGATE_H_

#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/html_viewer/html_frame.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace html_viewer {

class GlobalState;
class HTMLDocument;
class HTMLDocumentOOPIF;

// ApplicationDelegate created by the content handler for a specific url.
class HTMLDocumentApplicationDelegate : public mojo::ApplicationDelegate {
 public:
  HTMLDocumentApplicationDelegate(
      mojo::InterfaceRequest<mojo::Application> request,
      mojo::URLResponsePtr response,
      GlobalState* global_state,
      scoped_ptr<mojo::AppRefCount> parent_app_refcount);

  using HTMLFrameCreationCallback =
      base::Callback<HTMLFrame*(HTMLFrame::CreateParams*)>;

  void SetHTMLFrameCreationCallback(const HTMLFrameCreationCallback& callback);

 private:
  class ServiceConnectorQueue;

  ~HTMLDocumentApplicationDelegate() override;

  // Callback from the quit closure. We key off this rather than
  // ApplicationDelegate::Quit() as we don't want to shut down the messageloop
  // when we quit (the messageloop is shared among multiple
  // HTMLDocumentApplicationDelegates).
  void OnTerminate();

  // ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  void OnHTMLDocumentDeleted(HTMLDocument* document);
  void OnHTMLDocumentDeleted2(HTMLDocumentOOPIF* document);
  void OnResponseReceived(mojo::URLLoaderPtr loader,
                          mojo::ApplicationConnection* connection,
                          scoped_ptr<ServiceConnectorQueue> connector_queue,
                          mojo::URLResponsePtr response);

  mojo::ApplicationImpl app_;
  // AppRefCount of the parent (HTMLViewer).
  scoped_ptr<mojo::AppRefCount> parent_app_refcount_;
  const mojo::String url_;
  mojo::URLLoaderFactoryPtr url_loader_factory_;
  mojo::URLResponsePtr initial_response_;
  GlobalState* global_state_;

  // As we create HTMLDocuments they are added here. They are removed when the
  // HTMLDocument is deleted.
  std::set<HTMLDocument*> documents_;

  // As we create HTMLDocuments they are added here. They are removed when the
  // HTMLDocument is deleted.
  std::set<HTMLDocumentOOPIF*> documents2_;

  HTMLFrameCreationCallback html_frame_creation_callback_;

  base::WeakPtrFactory<HTMLDocumentApplicationDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HTMLDocumentApplicationDelegate);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_APPLICATION_DELEGATE_H_
