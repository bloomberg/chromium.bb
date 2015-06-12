// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_H_
#define COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_H_

#include <map>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "components/html_viewer/ax_provider_impl.h"
#include "components/html_viewer/frame_tree_manager.h"
#include "components/html_viewer/touch_handler.h"
#include "components/view_manager/public/cpp/view_manager_client_factory.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "mandoline/services/navigation/public/interfaces/navigation.mojom.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "mojo/application/public/cpp/app_lifetime_helper.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/application/public/cpp/lazy_interface_ptr.h"
#include "mojo/application/public/cpp/service_provider_impl.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"
#include "third_party/WebKit/public/web/WebViewClient.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_impl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace mojo {
class ViewManager;
class View;
}

namespace html_viewer {

class AxProviderImpl;
class DevToolsAgentImpl;
class Setup;
class WebLayerTreeViewImpl;

// A view for a single HTML document.
class HTMLDocument : public blink::WebViewClient,
                     public blink::WebFrameClient,
                     public mojo::ViewManagerDelegate,
                     public mojo::ViewObserver,
                     public mojo::InterfaceFactory<mojo::AxProvider>,
                     public mojo::InterfaceFactory<mandoline::FrameTreeClient> {
 public:
  // Load a new HTMLDocument with |response|.
  // |html_document_app| is the application this app was created in, and
  // |connection| the specific connection triggering this new instance.
  // |setup| is used to obtain init type state (such as resources).
  HTMLDocument(mojo::ApplicationImpl* html_document_app,
               mojo::ApplicationConnection* connection,
               mojo::URLResponsePtr response,
               Setup* setup);
  ~HTMLDocument() override;

 private:
  // Data associated with a child iframe.
  struct ChildFrameData {
    mojo::View* view;
    blink::WebTreeScopeType scope;
  };

  using FrameToViewMap = std::map<blink::WebLocalFrame*, ChildFrameData>;

  // Updates the size and scale factor of the webview and related classes from
  // |root_|.
  void UpdateWebviewSizeFromViewSize();

  void InitSetupAndLoadIfNecessary();

  // WebViewClient methods:
  virtual blink::WebStorageNamespace* createSessionStorageNamespace();

  // WebWidgetClient methods:
  void initializeLayerTreeView() override;
  blink::WebLayerTreeView* layerTreeView() override;

  // WebFrameClient methods:
  virtual blink::WebMediaPlayer* createMediaPlayer(
      blink::WebLocalFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client,
      blink::WebContentDecryptionModule* initial_cdm);
  virtual blink::WebFrame* createChildFrame(
      blink::WebLocalFrame* parent,
      blink::WebTreeScopeType scope,
      const blink::WebString& frameName,
      blink::WebSandboxFlags sandboxFlags);
  virtual void frameDetached(blink::WebFrame* frame);
  virtual void frameDetached(blink::WebFrame* frame, DetachType type);
  virtual blink::WebCookieJar* cookieJar(blink::WebLocalFrame* frame);
  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      const NavigationPolicyInfo& info);

  virtual void didAddMessageToConsole(const blink::WebConsoleMessage& message,
                                      const blink::WebString& source_name,
                                      unsigned source_line,
                                      const blink::WebString& stack_trace);
  virtual void didFinishLoad(blink::WebLocalFrame* frame);
  virtual void didNavigateWithinPage(blink::WebLocalFrame* frame,
                                     const blink::WebHistoryItem& history_item,
                                     blink::WebHistoryCommitType commit_type);
  virtual blink::WebEncryptedMediaClient* encryptedMediaClient();

  // ViewManagerDelegate methods:
  void OnEmbed(mojo::View* root) override;
  void OnViewManagerDestroyed(mojo::ViewManager* view_manager) override;

  // ViewObserver methods:
  void OnViewBoundsChanged(mojo::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override;
  void OnViewViewportMetricsChanged(
      mojo::View* view,
      const mojo::ViewportMetrics& old_metrics,
      const mojo::ViewportMetrics& new_metrics) override;
  void OnViewDestroyed(mojo::View* view) override;
  void OnViewInputEvent(mojo::View* view, const mojo::EventPtr& event) override;

  // mojo::InterfaceFactory<mojo::AxProvider>
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::AxProvider> request) override;

  // mojo::InterfaceFactory<mandoline::FrameTreeClient>
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mandoline::FrameTreeClient> request) override;

  void Load(mojo::URLResponsePtr response);

  // Converts a WebLocalFrame to a WebRemoteFrame. Used once we know the
  // url of a frame to trigger the navigation.
  void ConvertLocalFrameToRemoteFrame(blink::WebLocalFrame* frame);

  scoped_ptr<mojo::AppRefCount> app_refcount_;
  mojo::ApplicationImpl* html_document_app_;
  mojo::URLResponsePtr response_;
  mojo::LazyInterfacePtr<mojo::NavigatorHost> navigator_host_;
  blink::WebView* web_view_;
  mojo::View* root_;
  mojo::ViewManagerClientFactory view_manager_client_factory_;
  scoped_ptr<WebLayerTreeViewImpl> web_layer_tree_view_impl_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_thread_;

  // HTMLDocument owns these pointers; binding requests after document load.
  std::set<mojo::InterfaceRequest<mojo::AxProvider>*> ax_provider_requests_;
  std::set<AxProviderImpl*> ax_providers_;

  // A flag set on didFinishLoad.
  bool did_finish_load_ = false;

  Setup* setup_;

  scoped_ptr<TouchHandler> touch_handler_;

  FrameToViewMap frame_to_view_;

  FrameTreeManager frame_tree_manager_;
  mojo::Binding<mandoline::FrameTreeClient> frame_tree_manager_binding_;

  scoped_ptr<DevToolsAgentImpl> devtools_agent_;

  DISALLOW_COPY_AND_ASSIGN(HTMLDocument);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_H_
