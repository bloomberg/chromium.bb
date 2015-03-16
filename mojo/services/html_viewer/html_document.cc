// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/html_document.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "media/blink/webencryptedmediaclient_impl.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/filters/default_media_permission.h"
#include "mojo/services/html_viewer/blink_input_events_type_converters.h"
#include "mojo/services/html_viewer/blink_url_request_type_converters.h"
#include "mojo/services/html_viewer/weblayertreeview_impl.h"
#include "mojo/services/html_viewer/webmediaplayer_factory.h"
#include "mojo/services/html_viewer/webstoragenamespace_impl.h"
#include "mojo/services/html_viewer/weburlloader_impl.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/mojo/src/mojo/public/cpp/application/connect.h"
#include "third_party/mojo/src/mojo/public/cpp/system/data_pipe.h"
#include "third_party/mojo/src/mojo/public/interfaces/application/shell.mojom.h"
#include "third_party/mojo_services/src/surfaces/public/interfaces/surfaces.mojom.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"

using mojo::AxProvider;
using mojo::Rect;
using mojo::ServiceProviderPtr;
using mojo::URLResponsePtr;
using mojo::View;
using mojo::ViewManager;
using mojo::WeakBindToRequest;

namespace html_viewer {
namespace {

void ConfigureSettings(blink::WebSettings* settings) {
  settings->setCookieEnabled(true);
  settings->setDefaultFixedFontSize(13);
  settings->setDefaultFontSize(16);
  settings->setLoadsImagesAutomatically(true);
  settings->setJavaScriptEnabled(true);
}

mojo::Target WebNavigationPolicyToNavigationTarget(
    blink::WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyCurrentTab:
      return mojo::TARGET_SOURCE_NODE;
    case blink::WebNavigationPolicyNewBackgroundTab:
    case blink::WebNavigationPolicyNewForegroundTab:
    case blink::WebNavigationPolicyNewWindow:
    case blink::WebNavigationPolicyNewPopup:
      return mojo::TARGET_NEW_NODE;
    default:
      return mojo::TARGET_DEFAULT;
  }
}

bool CanNavigateLocally(blink::WebFrame* frame,
                        const blink::WebURLRequest& request) {
  // For now, we just load child frames locally.
  // TODO(aa): In the future, this should use embedding to connect to a
  // different instance of Blink if the frame is cross-origin.
  if (frame->parent())
    return true;

  // If we have extraData() it means we already have the url response
  // (presumably because we are being called via Navigate()). In that case we
  // can go ahead and navigate locally.
  if (request.extraData())
    return true;

  // Otherwise we don't know if we're the right app to handle this request. Ask
  // host to do the navigation for us.
  return false;
}

}  // namespace

HTMLDocument::HTMLDocument(
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    URLResponsePtr response,
    mojo::Shell* shell,
    scoped_refptr<base::MessageLoopProxy> compositor_thread,
    WebMediaPlayerFactory* web_media_player_factory,
    bool is_headless)
    : response_(response.Pass()),
      shell_(shell),
      web_view_(nullptr),
      root_(nullptr),
      view_manager_client_factory_(shell_, this),
      compositor_thread_(compositor_thread),
      web_media_player_factory_(web_media_player_factory),
      is_headless_(is_headless) {
  exported_services_.AddService(this);
  exported_services_.AddService(&view_manager_client_factory_);
  exported_services_.Bind(services.Pass());
  Load(response_.Pass());
}

HTMLDocument::~HTMLDocument() {
  STLDeleteElements(&ax_provider_impls_);

  if (web_view_)
    web_view_->close();
  if (root_)
    root_->RemoveObserver(this);
}

void HTMLDocument::OnEmbed(
    View* root,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services) {
  DCHECK(!is_headless_);
  root_ = root;
  embedder_service_provider_ = exposed_services.Pass();
  navigator_host_.set_service_provider(embedder_service_provider_.get());

  blink::WebSize root_size(root_->bounds().width, root_->bounds().height);
  web_view_->resize(root_size);
  web_layer_tree_view_impl_->setViewportSize(root_size);
  web_layer_tree_view_impl_->set_view(root_);
  root_->AddObserver(this);
}

void HTMLDocument::Create(mojo::ApplicationConnection* connection,
                          mojo::InterfaceRequest<AxProvider> request) {
  if (!web_view_)
    return;
  ax_provider_impls_.insert(
      WeakBindToRequest(new AxProviderImpl(web_view_), &request));
}

void HTMLDocument::OnViewManagerDisconnected(ViewManager* view_manager) {
  // TODO(aa): Need to figure out how shutdown works.
}

void HTMLDocument::Load(URLResponsePtr response) {
  web_view_ = blink::WebView::create(this);
  web_layer_tree_view_impl_->set_widget(web_view_);
  ConfigureSettings(web_view_->settings());
  web_view_->setMainFrame(blink::WebLocalFrame::create(this));

  GURL url(response->url);

  WebURLRequestExtraData* extra_data = new WebURLRequestExtraData;
  extra_data->synthetic_response = response.Pass();

  blink::WebURLRequest web_request;
  web_request.initialize();
  web_request.setURL(url);
  web_request.setExtraData(extra_data);

  web_view_->mainFrame()->loadRequest(web_request);
}

blink::WebStorageNamespace* HTMLDocument::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

void HTMLDocument::initializeLayerTreeView() {
  if (is_headless_) {
    web_layer_tree_view_impl_.reset(
        new WebLayerTreeViewImpl(compositor_thread_, nullptr, nullptr));
    return;
  }

  ServiceProviderPtr surfaces_service_provider;
  shell_->ConnectToApplication("mojo:surfaces_service",
                               GetProxy(&surfaces_service_provider), nullptr);
  mojo::SurfacePtr surface;
  ConnectToService(surfaces_service_provider.get(), &surface);

  ServiceProviderPtr gpu_service_provider;
  // TODO(jamesr): Should be mojo:gpu_service
  shell_->ConnectToApplication("mojo:native_viewport_service",
                               GetProxy(&gpu_service_provider), nullptr);
  mojo::GpuPtr gpu_service;
  ConnectToService(gpu_service_provider.get(), &gpu_service);
  web_layer_tree_view_impl_.reset(new WebLayerTreeViewImpl(
      compositor_thread_, surface.Pass(), gpu_service.Pass()));
}

blink::WebLayerTreeView* HTMLDocument::layerTreeView() {
  return web_layer_tree_view_impl_.get();
}

blink::WebMediaPlayer* HTMLDocument::createMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client) {
  return createMediaPlayer(frame, url, client, nullptr);
}

blink::WebMediaPlayer* HTMLDocument::createMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client,
    blink::WebContentDecryptionModule* initial_cdm) {
  if (!media_permission_)
    media_permission_.reset(new media::DefaultMediaPermission(true));

  blink::WebMediaPlayer* player =
      web_media_player_factory_
          ? web_media_player_factory_->CreateMediaPlayer(
                frame, url, client, media_permission_.get(), initial_cdm,
                shell_)
          : nullptr;
  return player;
}

blink::WebFrame* HTMLDocument::createChildFrame(
    blink::WebLocalFrame* parent,
    const blink::WebString& frameName,
    blink::WebSandboxFlags sandboxFlags) {
  blink::WebLocalFrame* web_frame = blink::WebLocalFrame::create(this);
  parent->appendChild(web_frame);
  return web_frame;
}

void HTMLDocument::frameDetached(blink::WebFrame* frame) {
  if (frame->parent())
    frame->parent()->removeChild(frame);

  // |frame| is invalid after here.
  frame->close();
}

blink::WebCookieJar* HTMLDocument::cookieJar(blink::WebLocalFrame* frame) {
  // TODO(darin): Blink does not fallback to the Platform provided WebCookieJar.
  // Either it should, as it once did, or we should find another solution here.
  return blink::Platform::current()->cookieJar();
}

blink::WebNavigationPolicy HTMLDocument::decidePolicyForNavigation(
    blink::WebLocalFrame* frame,
    blink::WebDataSource::ExtraData* data,
    const blink::WebURLRequest& request,
    blink::WebNavigationType nav_type,
    blink::WebNavigationPolicy default_policy,
    bool is_redirect) {
  if (CanNavigateLocally(frame, request))
    return default_policy;

  if (navigator_host_.get()) {
    navigator_host_->RequestNavigate(
        WebNavigationPolicyToNavigationTarget(default_policy),
        mojo::URLRequest::From(request).Pass());
  }

  return blink::WebNavigationPolicyIgnore;
}

void HTMLDocument::didAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
  VLOG(1) << "[" << source_name.utf8() << "(" << source_line << ")] "
          << message.text.utf8();
}

void HTMLDocument::didNavigateWithinPage(
    blink::WebLocalFrame* frame,
    const blink::WebHistoryItem& history_item,
    blink::WebHistoryCommitType commit_type) {
  if (navigator_host_.get())
    navigator_host_->DidNavigateLocally(history_item.urlString().utf8());
}

blink::WebEncryptedMediaClient* HTMLDocument::encryptedMediaClient() {
  if (!web_encrypted_media_client_) {
    if (!media_permission_)
      media_permission_.reset(new media::DefaultMediaPermission(true));
    web_encrypted_media_client_.reset(new media::WebEncryptedMediaClientImpl(
        make_scoped_ptr(new media::DefaultCdmFactory()),
        media_permission_.get()));
  }
  return web_encrypted_media_client_.get();
}

void HTMLDocument::OnViewBoundsChanged(View* view,
                                       const Rect& old_bounds,
                                       const Rect& new_bounds) {
  DCHECK_EQ(view, root_);
  web_view_->resize(
      blink::WebSize(view->bounds().width, view->bounds().height));
}

void HTMLDocument::OnViewDestroyed(View* view) {
  DCHECK_EQ(view, root_);
  root_ = nullptr;
}

void HTMLDocument::OnViewInputEvent(View* view, const mojo::EventPtr& event) {
  scoped_ptr<blink::WebInputEvent> web_event =
      event.To<scoped_ptr<blink::WebInputEvent>>();
  if (web_event)
    web_view_->handleInputEvent(*web_event);
}

}  // namespace html_viewer
