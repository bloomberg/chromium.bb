// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_document.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "components/devtools_service/public/cpp/switches.h"
#include "components/html_viewer/blink_input_events_type_converters.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/devtools_agent_impl.h"
#include "components/html_viewer/geolocation_client_impl.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/media_factory.h"
#include "components/html_viewer/web_layer_tree_view_impl.h"
#include "components/html_viewer/web_storage_namespace_impl.h"
#include "components/html_viewer/web_url_loader_impl.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "components/view_manager/public/cpp/view_property.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"
#include "third_party/WebKit/public/web/WebRemoteFrameClient.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/mojo/src/mojo/public/cpp/system/data_pipe.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"

using blink::WebString;
using mojo::AxProvider;
using mojo::Rect;
using mojo::ServiceProviderPtr;
using mojo::URLResponsePtr;
using mojo::View;
using mojo::ViewManager;
using mojo::WeakBindToRequest;

namespace html_viewer {
namespace {

bool EnableRemoteDebugging() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      devtools_service::kRemoteDebuggingPort);
}

// WebRemoteFrameClient implementation used for OOPIFs.
// TODO(sky): this needs to talk to browser by way of an interface.
class RemoteFrameClientImpl : public blink::WebRemoteFrameClient {
 public:
  explicit RemoteFrameClientImpl(mojo::View* view) : view_(view) {}
  ~RemoteFrameClientImpl() {}

  // WebRemoteFrameClient methods:
  virtual void postMessageEvent(blink::WebLocalFrame* source_frame,
                                blink::WebRemoteFrame* target_frame,
                                blink::WebSecurityOrigin target_origin,
                                blink::WebDOMMessageEvent event) {}
  virtual void initializeChildFrame(const blink::WebRect& frame_rect,
                                    float scale_factor) {
    mojo::Rect rect;
    rect.x = frame_rect.x;
    rect.y = frame_rect.y;
    rect.width = frame_rect.width;
    rect.height = frame_rect.height;
    view_->SetBounds(rect);
  }
  virtual void navigate(const blink::WebURLRequest& request,
                        bool should_replace_current_entry) {}
  virtual void reload(bool ignore_cache, bool is_client_redirect) {}

  virtual void forwardInputEvent(const blink::WebInputEvent* event) {}

 private:
  mojo::View* const view_;

  DISALLOW_COPY_AND_ASSIGN(RemoteFrameClientImpl);
};

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

HTMLDocument::CreateParams::CreateParams(
    mojo::ApplicationImpl* html_document_app,
    mojo::ApplicationConnection* connection,
    mojo::URLResponsePtr response,
    GlobalState* global_state,
    const DeleteCallback& delete_callback)
    : html_document_app(html_document_app),
      connection(connection),
      response(response.Pass()),
      global_state(global_state),
      delete_callback(delete_callback) {
}

HTMLDocument::CreateParams::~CreateParams() {
}

HTMLDocument::HTMLDocument(HTMLDocument::CreateParams* params)
    : app_refcount_(params->html_document_app->app_lifetime_helper()
                        ->CreateAppRefCount()),
      html_document_app_(params->html_document_app),
      response_(params->response.Pass()),
      navigator_host_(params->connection->GetServiceProvider()),
      web_view_(nullptr),
      root_(nullptr),
      view_manager_client_factory_(params->html_document_app->shell(), this),
      global_state_(params->global_state),
      delete_callback_(params->delete_callback) {
  params->connection->AddService(
      static_cast<InterfaceFactory<mojo::AxProvider>*>(this));
  params->connection->AddService(&view_manager_client_factory_);

  if (global_state_->did_init())
    Load(response_.Pass());
}

void HTMLDocument::Destroy() {
  // See comment in header for a description of lifetime.
  if (root_) {
    // Deleting the ViewManager calls back to OnViewManagerDestroyed() and
    // triggers deletion.
    delete root_->view_manager();
  } else {
    delete this;
  }
}

HTMLDocument::~HTMLDocument() {
  delete_callback_.Run(this);

  STLDeleteElements(&ax_providers_);
  STLDeleteElements(&ax_provider_requests_);

  if (web_view_)
    web_view_->close();
  if (root_)
    root_->RemoveObserver(this);
}

void HTMLDocument::OnEmbed(View* root) {
  DCHECK(!global_state_->is_headless());
  root_ = root;
  root_->AddObserver(this);
  UpdateFocus();

  InitGlobalStateAndLoadIfNecessary();
}

void HTMLDocument::OnViewManagerDestroyed(ViewManager* view_manager) {
  delete this;
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
        new AxProviderImpl(web_view_, request.Pass()));
  }
}

void HTMLDocument::Load(URLResponsePtr response) {
  DCHECK(!web_view_);
  web_view_ = blink::WebView::create(this);
  touch_handler_.reset(new TouchHandler(web_view_));
  web_layer_tree_view_impl_->set_widget(web_view_);
  ConfigureSettings(web_view_->settings());

  blink::WebLocalFrame* main_frame =
      blink::WebLocalFrame::create(blink::WebTreeScopeType::Document, this);
  web_view_->setMainFrame(main_frame);

  // TODO(yzshen): http://crbug.com/498986 Creating DevToolsAgentImpl instances
  // causes html_viewer_apptests flakiness currently. Before we fix that we
  // cannot enable remote debugging (which is required by Telemetry tests) on
  // the bots.
  if (EnableRemoteDebugging()) {
    devtools_agent_.reset(
        new DevToolsAgentImpl(main_frame, html_document_app_->shell()));
  }

  GURL url(response->url);

  WebURLRequestExtraData* extra_data = new WebURLRequestExtraData;
  extra_data->synthetic_response = response.Pass();

  blink::WebURLRequest web_request;
  web_request.initialize();
  web_request.setURL(url);
  web_request.setExtraData(extra_data);

  web_view_->mainFrame()->loadRequest(web_request);
  UpdateFocus();
}

void HTMLDocument::UpdateWebviewSizeFromViewSize() {
  web_view_->setDeviceScaleFactor(global_state_->device_pixel_ratio());
  const gfx::Size size_in_pixels(root_->bounds().width, root_->bounds().height);
  const gfx::Size size_in_dips = gfx::ConvertSizeToDIP(
      root_->viewport_metrics().device_pixel_ratio, size_in_pixels);
  web_view_->resize(
      blink::WebSize(size_in_dips.width(), size_in_dips.height()));
  web_layer_tree_view_impl_->setViewportSize(size_in_pixels);
}

void HTMLDocument::InitGlobalStateAndLoadIfNecessary() {
  DCHECK(root_);
  if (root_->viewport_metrics().device_pixel_ratio == 0.f)
    return;

  if (!web_view_) {
    global_state_->InitIfNecessary(
        root_->viewport_metrics().size_in_pixels.To<gfx::Size>(),
        root_->viewport_metrics().device_pixel_ratio);
    Load(response_.Pass());
  }

  UpdateWebviewSizeFromViewSize();
  web_layer_tree_view_impl_->set_view(root_);
}

blink::WebStorageNamespace* HTMLDocument::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

void HTMLDocument::initializeLayerTreeView() {
  if (global_state_->is_headless()) {
    web_layer_tree_view_impl_.reset(
        new WebLayerTreeViewImpl(global_state_->compositor_thread(), nullptr,
                                 nullptr, nullptr, nullptr));
    return;
  }

  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:surfaces_service");
  mojo::SurfacePtr surface;
  html_document_app_->ConnectToService(request.Pass(), &surface);

  // TODO(jamesr): Should be mojo:gpu_service
  mojo::URLRequestPtr request2(mojo::URLRequest::New());
  request2->url = mojo::String::From("mojo:view_manager");
  mojo::GpuPtr gpu_service;
  html_document_app_->ConnectToService(request2.Pass(), &gpu_service);
  web_layer_tree_view_impl_.reset(new WebLayerTreeViewImpl(
      global_state_->compositor_thread(),
      global_state_->gpu_memory_buffer_manager(),
      global_state_->raster_thread_helper()->task_graph_runner(),
      surface.Pass(), gpu_service.Pass()));
}

blink::WebLayerTreeView* HTMLDocument::layerTreeView() {
  return web_layer_tree_view_impl_.get();
}

blink::WebMediaPlayer* HTMLDocument::createMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    blink::WebContentDecryptionModule* initial_cdm) {
  return global_state_->media_factory()->CreateMediaPlayer(
      frame, url, client, encrypted_client, initial_cdm,
      html_document_app_->shell());
}

blink::WebFrame* HTMLDocument::createChildFrame(
    blink::WebLocalFrame* parent,
    blink::WebTreeScopeType scope,
    const blink::WebString& frameName,
    blink::WebSandboxFlags sandboxFlags) {
  blink::WebLocalFrame* child_frame = blink::WebLocalFrame::create(scope, this);
  parent->appendChild(child_frame);
  return child_frame;
}

void HTMLDocument::frameDetached(blink::WebFrame* frame, DetachType type) {
  DCHECK(type == DetachType::Remove);
  if (frame->parent())
    frame->parent()->removeChild(frame);

  if (devtools_agent_ && frame == devtools_agent_->frame())
    devtools_agent_.reset();

  // |frame| is invalid after here.
  frame->close();
}

blink::WebCookieJar* HTMLDocument::cookieJar(blink::WebLocalFrame* frame) {
  // TODO(darin): Blink does not fallback to the Platform provided WebCookieJar.
  // Either it should, as it once did, or we should find another solution here.
  return blink::Platform::current()->cookieJar();
}

blink::WebNavigationPolicy HTMLDocument::decidePolicyForNavigation(
    const NavigationPolicyInfo& info) {
  // TODO(yzshen): Force the current instance to handle all mainframe
  // navigations locally, if remote debugging is enabled. Otherwise, we will
  // lose DevTools agent state established in this HTMLDocument.
  // This can be removed once crbug.com/517266 is fixed.
  if (devtools_agent_ && devtools_agent_->frame() == info.frame)
    return info.defaultPolicy;

  std::string frame_name = info.frame ? info.frame->assignedName().utf8() : "";

  if (CanNavigateLocally(info.frame, info.urlRequest))
    return info.defaultPolicy;

  if (navigator_host_.get()) {
    mojo::URLRequestPtr url_request = mojo::URLRequest::From(info.urlRequest);
    navigator_host_->RequestNavigate(
        WebNavigationPolicyToNavigationTarget(info.defaultPolicy),
        url_request.Pass());
  }

  return blink::WebNavigationPolicyIgnore;
}

blink::WebGeolocationClient* HTMLDocument::geolocationClient() {
  if (!geolocation_client_impl_)
    geolocation_client_impl_.reset(new GeolocationClientImpl);
  return geolocation_client_impl_.get();
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
    ax_providers_.insert(new AxProviderImpl(web_view_, it->Pass()));
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
  return global_state_->media_factory()->GetEncryptedMediaClient();
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
  InitGlobalStateAndLoadIfNecessary();
}

void HTMLDocument::OnViewDestroyed(View* view) {
  DCHECK_EQ(view, root_);
  root_ = nullptr;
}

void HTMLDocument::OnViewInputEvent(View* view, const mojo::EventPtr& event) {
  if (event->pointer_data) {
    // Blink expects coordintes to be in DIPs.
    event->pointer_data->x /= global_state_->device_pixel_ratio();
    event->pointer_data->y /= global_state_->device_pixel_ratio();
    event->pointer_data->screen_x /= global_state_->device_pixel_ratio();
    event->pointer_data->screen_y /= global_state_->device_pixel_ratio();
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

void HTMLDocument::OnViewFocusChanged(mojo::View* gained_focus,
                                      mojo::View* lost_focus) {
  UpdateFocus();
}

void HTMLDocument::UpdateFocus() {
  if (!web_view_)
    return;
  bool is_focused = root_ && root_->HasFocus();
  web_view_->setFocus(is_focused);
  web_view_->setIsActive(is_focused);
}

}  // namespace html_viewer
