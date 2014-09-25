// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/html_document_view.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/services/html_viewer/blink_input_events_type_converters.h"
#include "mojo/services/html_viewer/blink_url_request_type_converters.h"
#include "mojo/services/html_viewer/weblayertreeview_impl.h"
#include "mojo/services/html_viewer/webmediaplayer_factory.h"
#include "mojo/services/html_viewer/webstoragenamespace_impl.h"
#include "mojo/services/html_viewer/weburlloader_impl.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/interfaces/surfaces/surfaces_service.mojom.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace mojo {
namespace {

void ConfigureSettings(blink::WebSettings* settings) {
  settings->setCookieEnabled(true);
  settings->setDefaultFixedFontSize(13);
  settings->setDefaultFontSize(16);
  settings->setLoadsImagesAutomatically(true);
  settings->setJavaScriptEnabled(true);
}

Target WebNavigationPolicyToNavigationTarget(
    blink::WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyCurrentTab:
      return TARGET_SOURCE_NODE;
    case blink::WebNavigationPolicyNewBackgroundTab:
    case blink::WebNavigationPolicyNewForegroundTab:
    case blink::WebNavigationPolicyNewWindow:
    case blink::WebNavigationPolicyNewPopup:
      return TARGET_NEW_NODE;
    default:
      return TARGET_DEFAULT;
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

HTMLDocumentView::HTMLDocumentView(
    URLResponsePtr response,
    InterfaceRequest<ServiceProvider> service_provider_request,
    Shell* shell,
    scoped_refptr<base::MessageLoopProxy> compositor_thread,
    WebMediaPlayerFactory* web_media_player_factory)
    : shell_(shell),
      web_view_(NULL),
      root_(NULL),
      view_manager_client_factory_(shell, this),
      compositor_thread_(compositor_thread),
      web_media_player_factory_(web_media_player_factory),
      weak_factory_(this) {
  ServiceProviderImpl* exported_services = new ServiceProviderImpl();
  exported_services->AddService(&view_manager_client_factory_);
  BindToRequest(exported_services, &service_provider_request);
  Load(response.Pass());
}

HTMLDocumentView::~HTMLDocumentView() {
  if (web_view_)
    web_view_->close();
  if (root_)
    root_->RemoveObserver(this);
}

void HTMLDocumentView::OnEmbed(
    ViewManager* view_manager,
    View* root,
    ServiceProviderImpl* embedee_service_provider_impl,
    scoped_ptr<ServiceProvider> embedder_service_provider) {
  root_ = root;
  embedder_service_provider_ = embedder_service_provider.Pass();
  navigator_host_.set_service_provider(embedder_service_provider_.get());

  web_view_->resize(root_->bounds().size());
  web_layer_tree_view_impl_->setViewportSize(root_->bounds().size());
  web_layer_tree_view_impl_->set_view(root_);
  root_->AddObserver(this);
}

void HTMLDocumentView::OnViewManagerDisconnected(ViewManager* view_manager) {
  // TODO(aa): Need to figure out how shutdown works.
}

void HTMLDocumentView::Load(URLResponsePtr response) {
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

blink::WebStorageNamespace* HTMLDocumentView::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

void HTMLDocumentView::initializeLayerTreeView() {
  ServiceProviderPtr surfaces_service_provider;
  shell_->ConnectToApplication("mojo:mojo_surfaces_service",
                               Get(&surfaces_service_provider));
  InterfacePtr<SurfacesService> surfaces_service;
  ConnectToService(surfaces_service_provider.get(), &surfaces_service);

  ServiceProviderPtr gpu_service_provider;
  // TODO(jamesr): Should be mojo:mojo_gpu_service
  shell_->ConnectToApplication("mojo:mojo_native_viewport_service",
                               Get(&gpu_service_provider));
  InterfacePtr<Gpu> gpu_service;
  ConnectToService(gpu_service_provider.get(), &gpu_service);
  web_layer_tree_view_impl_.reset(new WebLayerTreeViewImpl(
      compositor_thread_, surfaces_service.Pass(), gpu_service.Pass()));
}

blink::WebLayerTreeView* HTMLDocumentView::layerTreeView() {
  return web_layer_tree_view_impl_.get();
}

blink::WebMediaPlayer* HTMLDocumentView::createMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client) {
  return web_media_player_factory_->CreateMediaPlayer(frame, url, client);
}

blink::WebMediaPlayer* HTMLDocumentView::createMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client,
    blink::WebContentDecryptionModule* initial_cdm) {
  return createMediaPlayer(frame, url, client);
}

blink::WebFrame* HTMLDocumentView::createChildFrame(
    blink::WebLocalFrame* parent,
    const blink::WebString& frameName) {
  blink::WebLocalFrame* web_frame = blink::WebLocalFrame::create(this);
  parent->appendChild(web_frame);
  return web_frame;
}

void HTMLDocumentView::frameDetached(blink::WebFrame* frame) {
  if (frame->parent())
    frame->parent()->removeChild(frame);

  // |frame| is invalid after here.
  frame->close();
}

blink::WebCookieJar* HTMLDocumentView::cookieJar(blink::WebLocalFrame* frame) {
  // TODO(darin): Blink does not fallback to the Platform provided WebCookieJar.
  // Either it should, as it once did, or we should find another solution here.
  return blink::Platform::current()->cookieJar();
}

blink::WebNavigationPolicy HTMLDocumentView::decidePolicyForNavigation(
    blink::WebLocalFrame* frame, blink::WebDataSource::ExtraData* data,
    const blink::WebURLRequest& request, blink::WebNavigationType nav_type,
    blink::WebNavigationPolicy default_policy, bool is_redirect) {
  if (CanNavigateLocally(frame, request))
    return default_policy;

  navigator_host_->RequestNavigate(
      WebNavigationPolicyToNavigationTarget(default_policy),
      URLRequest::From(request).Pass());

  return blink::WebNavigationPolicyIgnore;
}

void HTMLDocumentView::didAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
}

void HTMLDocumentView::didNavigateWithinPage(
    blink::WebLocalFrame* frame, const blink::WebHistoryItem& history_item,
    blink::WebHistoryCommitType commit_type) {
  navigator_host_->DidNavigateLocally(history_item.urlString().utf8());
}

void HTMLDocumentView::OnViewBoundsChanged(View* view,
                                           const gfx::Rect& old_bounds,
                                           const gfx::Rect& new_bounds) {
  DCHECK_EQ(view, root_);
  web_view_->resize(view->bounds().size());
}

void HTMLDocumentView::OnViewDestroyed(View* view) {
  DCHECK_EQ(view, root_);
  view->RemoveObserver(this);
  root_ = NULL;
}

void HTMLDocumentView::OnViewInputEvent(View* view, const EventPtr& event) {
  scoped_ptr<blink::WebInputEvent> web_event =
      event.To<scoped_ptr<blink::WebInputEvent> >();
  if (web_event)
    web_view_->handleInputEvent(*web_event);
}

}  // namespace mojo
