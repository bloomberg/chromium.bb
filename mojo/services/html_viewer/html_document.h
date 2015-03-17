// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_HTML_DOCUMENT_H_
#define MOJO_SERVICES_HTML_VIEWER_HTML_DOCUMENT_H_

#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/services/html_viewer/ax_provider_impl.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"
#include "third_party/WebKit/public/web/WebViewClient.h"
#include "third_party/mojo/src/mojo/public/cpp/application/interface_factory.h"
#include "third_party/mojo/src/mojo/public/cpp/application/lazy_interface_ptr.h"
#include "third_party/mojo/src/mojo/public/cpp/application/service_provider_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_impl.h"
#include "third_party/mojo/src/mojo/public/interfaces/application/application.mojom.h"
#include "third_party/mojo_services/src/content_handler/public/interfaces/content_handler.mojom.h"
#include "third_party/mojo_services/src/navigation/public/interfaces/navigation.mojom.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_manager_client_factory.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_manager_delegate.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_observer.h"

namespace base {
class MessageLoopProxy;
}

namespace media {
class MediaPermission;
class WebEncryptedMediaClientImpl;
}

namespace mojo {
class ViewManager;
class View;
}

namespace html_viewer {

class AxProviderImpl;
class WebLayerTreeViewImpl;
class WebMediaPlayerFactory;

// A view for a single HTML document.
class HTMLDocument : public blink::WebViewClient,
                     public blink::WebFrameClient,
                     public mojo::ViewManagerDelegate,
                     public mojo::ViewObserver,
                     public mojo::InterfaceFactory<mojo::AxProvider> {
 public:
  // Load a new HTMLDocument with |response|.
  //
  // |services| should be used to implement a ServiceProvider which exposes
  // services to the connecting application.
  // Commonly, the connecting application is the ViewManager and it will
  // request ViewManagerClient.
  //
  // |shell| is the Shell connection for this mojo::Application.
  HTMLDocument(mojo::InterfaceRequest<mojo::ServiceProvider> services,
               mojo::URLResponsePtr response,
               mojo::Shell* shell,
               scoped_refptr<base::MessageLoopProxy> compositor_thread,
               WebMediaPlayerFactory* web_media_player_factory,
               bool is_headless);
  virtual ~HTMLDocument();

 private:
  // Updates the size and scale factor of the webview and related classes from
  // |root_|.
  void UpdateWebviewSizeFromViewSize();

  // WebViewClient methods:
  virtual blink::WebStorageNamespace* createSessionStorageNamespace();

  // WebWidgetClient methods:
  virtual void initializeLayerTreeView();
  virtual blink::WebLayerTreeView* layerTreeView();

  // WebFrameClient methods:
  virtual blink::WebMediaPlayer* createMediaPlayer(
      blink::WebLocalFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client);
  virtual blink::WebMediaPlayer* createMediaPlayer(
      blink::WebLocalFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client,
      blink::WebContentDecryptionModule* initial_cdm);
  virtual blink::WebFrame* createChildFrame(
      blink::WebLocalFrame* parent,
      const blink::WebString& frameName,
      blink::WebSandboxFlags sandboxFlags);
  virtual void frameDetached(blink::WebFrame*);
  virtual blink::WebCookieJar* cookieJar(blink::WebLocalFrame* frame);
  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      blink::WebLocalFrame* frame,
      blink::WebDataSource::ExtraData* data,
      const blink::WebURLRequest& request,
      blink::WebNavigationType nav_type,
      blink::WebNavigationPolicy default_policy,
      bool isRedirect);
  virtual void didAddMessageToConsole(const blink::WebConsoleMessage& message,
                                      const blink::WebString& source_name,
                                      unsigned source_line,
                                      const blink::WebString& stack_trace);
  virtual void didNavigateWithinPage(blink::WebLocalFrame* frame,
                                     const blink::WebHistoryItem& history_item,
                                     blink::WebHistoryCommitType commit_type);
  virtual blink::WebEncryptedMediaClient* encryptedMediaClient();

  // ViewManagerDelegate methods:
  void OnEmbed(mojo::View* root,
               mojo::InterfaceRequest<mojo::ServiceProvider> services,
               mojo::ServiceProviderPtr exposed_services) override;
  void OnViewManagerDisconnected(mojo::ViewManager* view_manager) override;

  // ViewObserver methods:
  void OnViewBoundsChanged(mojo::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override;
  void OnViewDestroyed(mojo::View* view) override;
  void OnViewInputEvent(mojo::View* view, const mojo::EventPtr& event) override;

  // InterfaceFactory<AxProvider>
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::AxProvider> request) override;

  void Load(mojo::URLResponsePtr response);

  mojo::URLResponsePtr response_;
  mojo::ServiceProviderImpl exported_services_;
  mojo::ServiceProviderPtr embedder_service_provider_;
  mojo::Shell* shell_;
  mojo::LazyInterfacePtr<mojo::NavigatorHost> navigator_host_;
  blink::WebView* web_view_;
  mojo::View* root_;
  mojo::ViewManagerClientFactory view_manager_client_factory_;
  scoped_ptr<WebLayerTreeViewImpl> web_layer_tree_view_impl_;
  scoped_refptr<base::MessageLoopProxy> compositor_thread_;
  WebMediaPlayerFactory* web_media_player_factory_;

  // EncryptedMediaClient attached to this frame; lazily initialized.
  scoped_ptr<media::WebEncryptedMediaClientImpl> web_encrypted_media_client_;
  scoped_ptr<media::MediaPermission> media_permission_;

  // HTMLDocument owns these pointers.
  std::set<AxProviderImpl*> ax_provider_impls_;

  // Set if the content will never be displayed.
  bool is_headless_;

  DISALLOW_COPY_AND_ASSIGN(HTMLDocument);
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_HTML_DOCUMENT_H_
