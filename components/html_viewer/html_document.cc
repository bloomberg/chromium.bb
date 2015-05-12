// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_document.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "components/html_viewer/blink_input_events_type_converters.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/setup.h"
#include "components/html_viewer/web_layer_tree_view_impl.h"
#include "components/html_viewer/web_media_player_factory.h"
#include "components/html_viewer/web_storage_namespace_impl.h"
#include "components/html_viewer/web_url_loader_impl.h"
#include "components/surfaces/public/interfaces/surfaces.mojom.h"
#include "components/view_manager/public/cpp/view.h"
#include "media/blink/webencryptedmediaclient_impl.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/filters/default_media_permission.h"
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
#include "third_party/mojo/src/mojo/public/cpp/application/application_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/application/connect.h"
#include "third_party/mojo/src/mojo/public/cpp/system/data_pipe.h"
#include "third_party/mojo/src/mojo/public/interfaces/application/shell.mojom.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"

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

  // mojo::NavigatorHost doesn't accept POSTs, so for now reuse this instance.
  // TODO(jam): improve this (and copy logic from RenderFrameImpl's version)
  // when we have multi-process.
  if (EqualsASCII(request.httpMethod(), "POST"))
    return true;

  // Logging into Gmail fails when the referrer isn't sent with a request.
  // TODO(jam): pass referrer and other HTTP data to NavigatorHost so we can
  // use a new process in this case.
  if (!request.httpHeaderField(blink::WebString::fromUTF8("Referer")).isEmpty())
    return true;

  // Otherwise we don't know if we're the right app to handle this request. Ask
  // host to do the navigation for us.
  return false;
}

bool AreSecureCodecsSupported() {
  // Hardware-secure codecs are not currently supported by HTML Viewer on any
  // platform.
  return false;
}

}  // namespace

HTMLDocument::HTMLDocument(
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    URLResponsePtr response,
    mojo::Shell* shell,
    Setup* setup)
    : response_(response.Pass()),
      shell_(shell),
      web_view_(nullptr),
      root_(nullptr),
      view_manager_client_factory_(shell_, this),
      setup_(setup) {
  exported_services_.AddService(this);
  exported_services_.AddService(&view_manager_client_factory_);
  exported_services_.Bind(services.Pass());
  if (setup_->did_init())
    Load(response_.Pass());
}

HTMLDocument::~HTMLDocument() {
  STLDeleteElements(&ax_providers_);
  STLDeleteElements(&ax_provider_requests_);

  if (web_view_)
    web_view_->close();
  if (root_)
    root_->RemoveObserver(this);
}

void HTMLDocument::OnEmbed(
    View* root,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services) {
  DCHECK(!setup_->is_headless());
  root_ = root;
  root_->AddObserver(this);
  embedder_service_provider_ = exposed_services.Pass();
  navigator_host_.set_service_provider(embedder_service_provider_.get());

  InitSetupAndLoadIfNecessary();
}

void HTMLDocument::OnViewManagerDisconnected(ViewManager* view_manager) {
  // TODO(aa): Need to figure out how shutdown works.
}

void HTMLDocument::Create(mojo::ApplicationConnection* connection,
                          mojo::InterfaceRequest<AxProvider> request) {
  if (!did_finish_load_) {
    // Cache AxProvider interface requests until the document finishes loading.
    auto cached_request = new mojo::InterfaceRequest<AxProvider>();
    *cached_request = request.Pass();
    ax_provider_requests_.insert(cached_request);
  } else {
    ax_providers_.insert(
        WeakBindToRequest(new AxProviderImpl(web_view_), &request));
  }
}

void HTMLDocument::Load(URLResponsePtr response) {
  DCHECK(!web_view_);
  web_view_ = blink::WebView::create(this);
  touch_handler_.reset(new TouchHandler(web_view_));
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

void HTMLDocument::UpdateWebviewSizeFromViewSize() {
  web_view_->setDeviceScaleFactor(setup_->device_pixel_ratio());
  const gfx::Size size_in_pixels(root_->bounds().width, root_->bounds().height);
  const gfx::Size size_in_dips = gfx::ConvertSizeToDIP(
      root_->viewport_metrics().device_pixel_ratio, size_in_pixels);
  web_view_->resize(
      blink::WebSize(size_in_dips.width(), size_in_dips.height()));
  web_layer_tree_view_impl_->setViewportSize(size_in_pixels);
}

void HTMLDocument::InitSetupAndLoadIfNecessary() {
  DCHECK(root_);
  if (web_view_ || root_->viewport_metrics().device_pixel_ratio == 0.f)
    return;

  setup_->InitIfNecessary(gfx::Size(root_->viewport_metrics().size->width,
                                    root_->viewport_metrics().size->height),
                          root_->viewport_metrics().device_pixel_ratio);
  Load(response_.Pass());

  UpdateWebviewSizeFromViewSize();
  web_layer_tree_view_impl_->set_view(root_);
}

blink::WebStorageNamespace* HTMLDocument::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

void HTMLDocument::initializeLayerTreeView() {
  if (setup_->is_headless()) {
    web_layer_tree_view_impl_.reset(new WebLayerTreeViewImpl(
        setup_->compositor_thread(), nullptr, nullptr));
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
      setup_->compositor_thread(), surface.Pass(), gpu_service.Pass()));
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
  blink::WebMediaPlayer* player =
      setup_->web_media_player_factory()
          ? setup_->web_media_player_factory()->CreateMediaPlayer(
                frame, url, client, GetMediaPermission(), GetCdmFactory(),
                initial_cdm, shell_)
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

void HTMLDocument::didFinishLoad(blink::WebLocalFrame* frame) {
  // TODO(msw): Notify AxProvider clients of updates on child frame loads.
  did_finish_load_ = true;
  // Bind any pending AxProviderImpl interface requests.
  for (auto it : ax_provider_requests_)
    ax_providers_.insert(WeakBindToRequest(new AxProviderImpl(web_view_), it));
  STLDeleteElements(&ax_provider_requests_);
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
    web_encrypted_media_client_.reset(new media::WebEncryptedMediaClientImpl(
        base::Bind(&AreSecureCodecsSupported), GetCdmFactory(),
        GetMediaPermission()));
  }
  return web_encrypted_media_client_.get();
}

void HTMLDocument::OnViewBoundsChanged(View* view,
                                       const Rect& old_bounds,
                                       const Rect& new_bounds) {
  DCHECK_EQ(view, root_);
  UpdateWebviewSizeFromViewSize();
}

void HTMLDocument::OnViewViewportMetricsChanged(
    mojo::View* view,
    const mojo::ViewportMetrics& old_metrics,
    const mojo::ViewportMetrics& new_metrics) {
  InitSetupAndLoadIfNecessary();
}

void HTMLDocument::OnViewDestroyed(View* view) {
  DCHECK_EQ(view, root_);
  root_ = nullptr;
  delete this;
  mojo::ApplicationImpl::Terminate();
}

void HTMLDocument::OnViewInputEvent(View* view, const mojo::EventPtr& event) {
  if (event->pointer_data) {
    // Blink expects coordintes to be in DIPs.
    event->pointer_data->x /= setup_->device_pixel_ratio();
    event->pointer_data->y /= setup_->device_pixel_ratio();
    event->pointer_data->screen_x /= setup_->device_pixel_ratio();
    event->pointer_data->screen_y /= setup_->device_pixel_ratio();
  }

  if ((event->action == mojo::EVENT_TYPE_POINTER_DOWN ||
       event->action == mojo::EVENT_TYPE_POINTER_UP ||
       event->action == mojo::EVENT_TYPE_POINTER_CANCEL ||
       event->action == mojo::EVENT_TYPE_POINTER_MOVE) &&
      event->pointer_data->kind == mojo::POINTER_KIND_TOUCH) {
    touch_handler_->OnTouchEvent(*event);
    return;
  }
  scoped_ptr<blink::WebInputEvent> web_event =
      event.To<scoped_ptr<blink::WebInputEvent>>();
  if (web_event)
    web_view_->handleInputEvent(*web_event);
}

media::MediaPermission* HTMLDocument::GetMediaPermission() {
  if (!media_permission_)
    media_permission_.reset(new media::DefaultMediaPermission(true));
  return media_permission_.get();
}

media::CdmFactory* HTMLDocument::GetCdmFactory() {
  if (!cdm_factory_)
    cdm_factory_.reset(new media::DefaultCdmFactory());
  return cdm_factory_.get();
}

}  // namespace html_viewer
