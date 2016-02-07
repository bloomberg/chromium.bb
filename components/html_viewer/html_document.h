// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_H_
#define COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_H_

#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "components/html_viewer/ax_provider_impl.h"
#include "components/html_viewer/html_frame_delegate.h"
#include "components/html_viewer/public/interfaces/test_html_viewer.mojom.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/web_view/public/interfaces/frame.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/shell/public/cpp/app_lifetime_helper.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/service_provider_impl.h"
#include "mojo/shell/public/interfaces/application.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace mus {
class Window;
class WindowTreeConnection;
}

namespace html_viewer {

class AxProviderImpl;
class DocumentResourceWaiter;
class GlobalState;
class HTMLFactory;
class HTMLFrame;
class TestHTMLViewerImpl;
class WindowTreeDelegateImpl;
class WebLayerTreeViewImpl;

// A window for a single HTML document.
//
// HTMLDocument is deleted in one of two ways:
// . When the Window the HTMLDocument is embedded in is destroyed.
// . Explicitly by way of Destroy().
class HTMLDocument
    : public mus::WindowTreeDelegate,
      public HTMLFrameDelegate,
      public mojo::InterfaceFactory<mojo::AxProvider>,
      public mojo::InterfaceFactory<web_view::mojom::FrameClient>,
      public mojo::InterfaceFactory<TestHTMLViewer>,
      public mojo::InterfaceFactory<devtools_service::DevToolsAgent>,
      public mojo::InterfaceFactory<mus::mojom::WindowTreeClient> {
 public:
  using DeleteCallback = base::Callback<void(HTMLDocument*)>;

  // Load a new HTMLDocument with |response|.
  // |html_document_shell| is the application this app was created in, and
  // |connection| the specific connection triggering this new instance.
  // |setup| is used to obtain init type state (such as resources).
  HTMLDocument(mojo::Shell* html_document_shell,
               mojo::ApplicationConnection* connection,
               mojo::URLResponsePtr response,
               GlobalState* setup,
               const DeleteCallback& delete_callback,
               HTMLFactory* factory);

  // Deletes this object.
  void Destroy();

 private:
  friend class DocumentResourceWaiter;  // So it can call Load().

  // Requests for interfaces before the document is loaded go here. Once
  // loaded the requests are bound and BeforeLoadCache is deleted.
  struct BeforeLoadCache {
    BeforeLoadCache();
    ~BeforeLoadCache();

    std::set<mojo::InterfaceRequest<mojo::AxProvider>*> ax_provider_requests;
    std::set<mojo::InterfaceRequest<TestHTMLViewer>*> test_interface_requests;
  };

  // Any state that needs to be moved when rendering transfers from one frame
  // to another is stored here.
  struct TransferableState {
    TransferableState();
    ~TransferableState();

    // Takes the state from |other|.
    void Move(TransferableState* other);

    bool owns_window_tree_connection;
    mus::Window* root;
    scoped_ptr<WindowTreeDelegateImpl> window_tree_delegate_impl;
  };

  ~HTMLDocument() override;

  void Load();

  BeforeLoadCache* GetBeforeLoadCache();

  // WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;

  // HTMLFrameDelegate:
  mojo::Shell* GetShell() override;
  HTMLFactory* GetHTMLFactory() override;
  void OnFrameDidFinishLoad() override;
  void OnFrameSwappedToRemote() override;
  void OnSwap(HTMLFrame* frame, HTMLFrameDelegate* old_delegate) override;
  void OnFrameDestroyed() override;

  // mojo::InterfaceFactory<mojo::AxProvider>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::AxProvider> request) override;

  // mojo::InterfaceFactory<web_view::mojom::FrameClient>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<web_view::mojom::FrameClient> request) override;

  // mojo::InterfaceFactory<TestHTMLViewer>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<TestHTMLViewer> request) override;

  // mojo::InterfaceFactory<devtools_service::DevToolsAgent>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<devtools_service::DevToolsAgent> request) override;

  // mojo::InterfaceFactory<mus::WindowTreeClient>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request) override;

  scoped_ptr<mojo::AppRefCount> app_refcount_;
  mojo::Shell* html_document_shell_;
  mojo::ApplicationConnection* connection_;

  // HTMLDocument owns these pointers; binding requests after document load.
  std::set<AxProviderImpl*> ax_providers_;

  ScopedVector<TestHTMLViewerImpl> test_html_viewers_;

  // Set to true when the local frame has finished loading.
  bool did_finish_local_frame_load_ = false;

  GlobalState* global_state_;

  HTMLFrame* frame_;

  scoped_ptr<DocumentResourceWaiter> resource_waiter_;

  scoped_ptr<BeforeLoadCache> before_load_cache_;

  DeleteCallback delete_callback_;

  HTMLFactory* factory_;

  TransferableState transferable_state_;

  // Cache interface request of DevToolsAgent if |frame_| hasn't been
  // initialized.
  mojo::InterfaceRequest<devtools_service::DevToolsAgent>
      devtools_agent_request_;

  DISALLOW_COPY_AND_ASSIGN(HTMLDocument);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_H_
