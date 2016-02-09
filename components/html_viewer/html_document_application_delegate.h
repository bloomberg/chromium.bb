// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_APPLICATION_DELEGATE_H_
#define COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_APPLICATION_DELEGATE_H_

#include <set>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/html_viewer/html_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace html_viewer {

class GlobalState;
class HTMLDocument;

// mojo::ShellClient created by the content handler for a specific url.
class HTMLDocumentApplicationDelegate : public mojo::ShellClient,
                                        public HTMLFactory {
 public:
  HTMLDocumentApplicationDelegate(
      mojo::ShellClientRequest request,
      mojo::URLResponsePtr response,
      GlobalState* global_state,
      scoped_ptr<mojo::AppRefCount> parent_app_refcount,
      const mojo::Callback<void()>& destruct_callback);

  void set_html_factory(HTMLFactory* factory) { html_factory_ = factory; }
  HTMLFactory* html_factory() { return html_factory_; }

 private:
  class InterfaceBinderQueue;

  ~HTMLDocumentApplicationDelegate() override;

  // Callback from the quit closure. We key off this rather than
  // ShellClient::Quit() as we don't want to shut down the messageloop
  // when we quit (the messageloop is shared among multiple
  // HTMLDocumentApplicationDelegates).
  void OnTerminate();

  // mojo::ShellClient:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  void OnHTMLDocumentDeleted2(HTMLDocument* document);
  void OnResponseReceived(scoped_ptr<mojo::AppRefCount> app_refcount,
                          mojo::URLLoaderPtr loader,
                          mojo::Connection* connection,
                          scoped_ptr<InterfaceBinderQueue> binder_queue,
                          mojo::URLResponsePtr response);

  // HTMLFactory:
  HTMLFrame* CreateHTMLFrame(HTMLFrame::CreateParams* params) override;
  HTMLWidgetRootLocal* CreateHTMLWidgetRootLocal(
      HTMLWidgetRootLocal::CreateParams* params) override;

  mojo::ShellConnection app_;
  // AppRefCount of the parent (HTMLViewer).
  scoped_ptr<mojo::AppRefCount> parent_app_refcount_;
  const mojo::String url_;
  mojo::URLLoaderFactoryPtr url_loader_factory_;
  mojo::URLResponsePtr initial_response_;
  GlobalState* global_state_;

  // As we create HTMLDocuments they are added here. They are removed when the
  // HTMLDocument is deleted.
  std::set<HTMLDocument*> documents2_;

  HTMLFactory* html_factory_;

  mojo::Callback<void()> destruct_callback_;

  base::WeakPtrFactory<HTMLDocumentApplicationDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HTMLDocumentApplicationDelegate);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_APPLICATION_DELEGATE_H_
