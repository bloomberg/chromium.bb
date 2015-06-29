// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/frame.h"

#include <algorithm>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "components/html_viewer/ax_provider_impl.h"
#include "components/html_viewer/blink_input_events_type_converters.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/frame_tree_manager.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/media_factory.h"
#include "components/html_viewer/touch_handler.h"
#include "components/html_viewer/web_layer_tree_view_impl.h"
#include "components/html_viewer/web_storage_namespace_impl.h"
#include "components/html_viewer/web_url_loader_impl.h"
#include "components/view_manager/ids.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"
#include "mandoline/services/navigation/public/interfaces/navigation.mojom.h"
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
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
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

using mojo::AxProvider;
using mojo::Rect;
using mojo::ServiceProviderPtr;
using mojo::URLResponsePtr;
using mojo::View;
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

Frame* GetPreviousSibling(Frame* frame) {
  DCHECK(frame->parent());
  auto iter = std::find(frame->parent()->children().begin(),
                        frame->parent()->children().end(), frame);
  return (iter == frame->parent()->children().begin()) ? nullptr : *(--iter);
}

}  // namespace

Frame::Frame(const Frame::CreateParams& params)
    : frame_tree_manager_(params.manager),
      parent_(params.parent),
      view_(nullptr),
      id_(params.id),
      web_frame_(nullptr),
      web_widget_(nullptr),
      scope_(blink::WebTreeScopeType::Document),
      weak_factory_(this) {
  if (parent_)
    parent_->children_.push_back(this);
}

void Frame::Init(mojo::View* local_view) {
  // TODO(sky): need to plumb through scope, name and other args correctly for
  // frame creation.
  if (!parent_) {
    CreateWebWidget();
    // This is the root of the tree (aka the main frame).
    // Expected order for creating webframes is:
    // . Create local webframe (first webframe must always be local).
    // . Set as main frame on WebView.
    // . Swap to remote (if not local).
    blink::WebLocalFrame* local_web_frame =
        blink::WebLocalFrame::create(blink::WebTreeScopeType::Document, this);
    // We need to set the main frame before creating children so that state is
    // properly set up in blink.
    // TODO(sky): I don't like these casts.
    web_view()->setMainFrame(local_web_frame);
    const gfx::Size size_in_pixels(local_view->bounds().width,
                                   local_view->bounds().height);
    const gfx::Size size_in_dips = gfx::ConvertSizeToDIP(
        local_view->viewport_metrics().device_pixel_ratio, size_in_pixels);
    web_widget_->resize(size_in_dips);
    web_frame_ = local_web_frame;
    web_view()->setDeviceScaleFactor(global_state()->device_pixel_ratio());
    if (id_ != local_view->id()) {
      blink::WebRemoteFrame* remote_web_frame = blink::WebRemoteFrame::create(
          blink::WebTreeScopeType::Document, this);
      local_web_frame->swap(remote_web_frame);
      // local_web_frame->close();
      web_frame_ = remote_web_frame;
      // TODO(sky): This needs to come from FrameTreeData.
      remote_web_frame->setReplicatedOrigin(
          blink::WebSecurityOrigin::createFromString(""));
    }
  } else if (id_ == local_view->id()) {
    // Frame represents the local frame.
    Frame* previous_sibling = GetPreviousSibling(this);
    blink::WebFrame* previous_web_frame =
        previous_sibling ? previous_sibling->web_frame() : nullptr;
    DCHECK(!parent_->IsLocal());
    web_frame_ = parent_->web_frame()->toWebRemoteFrame()->createLocalChild(
        blink::WebTreeScopeType::Document, "", blink::WebSandboxFlags::None,
        this, previous_web_frame);
    CreateWebWidget();
  } else if (parent_->web_frame()->isWebLocalFrame()) {
    blink::WebLocalFrame* local_web_frame =
        blink::WebLocalFrame::create(blink::WebTreeScopeType::Document, this);
    parent_->web_frame()->appendChild(local_web_frame);
    blink::WebRemoteFrame* remote_web_frame =
        blink::WebRemoteFrame::create(blink::WebTreeScopeType::Document, this);
    // remote_web_frame->swap(local_web_frame);
    local_web_frame->close();
    web_frame_ = remote_web_frame;
  } else {
    web_frame_ = parent_->web_frame()->toWebRemoteFrame()->createRemoteChild(
        blink::WebTreeScopeType::Document, "", blink::WebSandboxFlags::None,
        this);
  }
}

void Frame::Close() {
  if (web_widget_) {
    // Closing the widget implicitly detaches the frame.
    web_widget_->close();
  } else {
    web_frame_->detach();
  }
}

const Frame* Frame::FindFrame(uint32_t id) const {
  if (id == id_)
    return this;

  for (const Frame* child : children_) {
    const Frame* match = child->FindFrame(id);
    if (match)
      return match;
  }
  return nullptr;
}

blink::WebView* Frame::web_view() {
  return web_widget_ && web_widget_->isWebView()
             ? static_cast<blink::WebView*>(web_widget_)
             : nullptr;
}

Frame::~Frame() {
  DCHECK(children_.empty());

  if (parent_) {
    auto iter =
        std::find(parent_->children_.begin(), parent_->children_.end(), this);
    parent_->children_.erase(iter);
  }
  parent_ = nullptr;

  frame_tree_manager_->OnFrameDestroyed(this);

  if (view_) {
    view_->RemoveObserver(this);
    view_->Destroy();
  }
}

bool Frame::IsLocal() const {
  return web_frame_->isWebLocalFrame();
}

void Frame::SetView(mojo::View* view) {
  DCHECK(!view_);
  view_ = view;
  view_->AddObserver(this);
}

void Frame::CreateWebWidget() {
  DCHECK(!web_widget_);
  if (parent_) {
    // TODO(sky): this isn't quite right. I should only have a WebFrameWidget
    // for local roots. And the cast to local fram definitely isn't right.
    web_widget_ =
        blink::WebFrameWidget::create(this, web_frame_->toWebLocalFrame());
  } else if (view_ && view_->id() == id_) {
    web_widget_ = blink::WebView::create(this);
  } else {
    web_widget_ = blink::WebView::create(nullptr);
  }

  // Creating the widget calls initializeLayerTreeView() to create the
  // |web_layer_tree_view_impl_|. As we haven't yet assigned the |web_widget_|
  // we have to set it here.
  if (web_layer_tree_view_impl_) {
    web_layer_tree_view_impl_->set_widget(web_widget_);
    web_layer_tree_view_impl_->set_view(view_);
    UpdateWebViewSizeFromViewSize();
  }

  if (web_view())
    ConfigureSettings(web_view()->settings());
}

void Frame::UpdateFocus() {
  if (!web_widget_ || !view_)
    return;
  const bool is_focused = view_ && view_->HasFocus();
  web_widget_->setFocus(is_focused);
  if (web_widget_->isWebView())
    static_cast<blink::WebView*>(web_widget_)->setIsActive(is_focused);
}

void Frame::UpdateWebViewSizeFromViewSize() {
  if (!web_widget_ || !view_)
    return;

  const gfx::Size size_in_pixels(view_->bounds().width, view_->bounds().height);
  const gfx::Size size_in_dips = gfx::ConvertSizeToDIP(
      view_->viewport_metrics().device_pixel_ratio, size_in_pixels);
  web_widget_->resize(
      blink::WebSize(size_in_dips.width(), size_in_dips.height()));
  web_layer_tree_view_impl_->setViewportSize(size_in_pixels);
}

void Frame::SwapToRemote(const blink::WebURLRequest& request) {
  DCHECK(IsLocal());
  mojo::URLRequestPtr url_request = mojo::URLRequest::From(request);
  view_->EmbedAllowingReembed(url_request.Pass());

  // TODO(sky): I tried swapping the frame types here, but that resulted in
  // the view never getting sized. Figure out why.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&Frame::FinishSwapToRemote, weak_factory_.GetWeakPtr()));
}

void Frame::FinishSwapToRemote() {
  if (web_frame_->isWebRemoteFrame())
    return;  // We already did the swap.

  blink::WebRemoteFrame* remote_frame =
      blink::WebRemoteFrame::create(scope_, this);
  remote_frame->initializeFromFrame(web_frame_->toWebLocalFrame());
  // swap() ends up calling us back and we then close the frame.
  web_frame_->swap(remote_frame);
  web_frame_ = remote_frame;
}

Frame* Frame::FindFrameWithWebFrame(blink::WebFrame* web_frame) {
  if (web_frame_ == web_frame)
    return this;
  for (Frame* child_frame : children_) {
    Frame* result = child_frame->FindFrameWithWebFrame(web_frame);
    if (result)
      return result;
  }
  return nullptr;
}

void Frame::FrameDetachedImpl(blink::WebFrame* web_frame) {
  DCHECK_EQ(web_frame_, web_frame);

  while (!children_.empty()) {
    Frame* child = children_.front();
    child->Close();
    DCHECK(children_.empty() || children_.front() != child);
  }

  if (web_frame->parent())
    web_frame->parent()->removeChild(web_frame);

  delete this;
}

void Frame::OnViewBoundsChanged(View* view,
                                const Rect& old_bounds,
                                const Rect& new_bounds) {
  DCHECK_EQ(view, view_);
  UpdateWebViewSizeFromViewSize();
}

void Frame::OnViewDestroyed(View* view) {
  DCHECK_EQ(view, view_);
  view_->RemoveObserver(this);
  view_ = nullptr;
  Close();
}

void Frame::OnViewInputEvent(View* view, const mojo::EventPtr& event) {
  if (event->pointer_data) {
    // Blink expects coordintes to be in DIPs.
    event->pointer_data->x /= global_state()->device_pixel_ratio();
    event->pointer_data->y /= global_state()->device_pixel_ratio();
    event->pointer_data->screen_x /= global_state()->device_pixel_ratio();
    event->pointer_data->screen_y /= global_state()->device_pixel_ratio();
  }

  if (!touch_handler_ && web_widget_)
    touch_handler_.reset(new TouchHandler(web_widget_));

  if ((event->action == mojo::EVENT_TYPE_POINTER_DOWN ||
       event->action == mojo::EVENT_TYPE_POINTER_UP ||
       event->action == mojo::EVENT_TYPE_POINTER_CANCEL ||
       event->action == mojo::EVENT_TYPE_POINTER_MOVE) &&
      event->pointer_data->kind == mojo::POINTER_KIND_TOUCH) {
    touch_handler_->OnTouchEvent(*event);
    return;
  }

  if (!web_widget_)
    return;

  scoped_ptr<blink::WebInputEvent> web_event =
      event.To<scoped_ptr<blink::WebInputEvent>>();
  if (web_event)
    web_widget_->handleInputEvent(*web_event);
}

void Frame::OnViewFocusChanged(mojo::View* gained_focus,
                               mojo::View* lost_focus) {
  UpdateFocus();
}

void Frame::initializeLayerTreeView() {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:surfaces_service");
  mojo::SurfacePtr surface;
  frame_tree_manager_->app()->ConnectToService(request.Pass(), &surface);

  // TODO(jamesr): Should be mojo:gpu_service
  mojo::URLRequestPtr request2(mojo::URLRequest::New());
  request2->url = mojo::String::From("mojo:view_manager");
  mojo::GpuPtr gpu_service;
  frame_tree_manager_->app()->ConnectToService(request2.Pass(), &gpu_service);
  web_layer_tree_view_impl_.reset(new WebLayerTreeViewImpl(
      global_state()->compositor_thread(),
      global_state()->gpu_memory_buffer_manager(),
      global_state()->raster_thread_helper()->task_graph_runner(),
      surface.Pass(), gpu_service.Pass()));
}

blink::WebLayerTreeView* Frame::layerTreeView() {
  return web_layer_tree_view_impl_.get();
}

blink::WebStorageNamespace* Frame::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

blink::WebMediaPlayer* Frame::createMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client,
    blink::WebContentDecryptionModule* initial_cdm) {
  return global_state()->media_factory()->CreateMediaPlayer(
      frame, url, client, initial_cdm, frame_tree_manager_->app()->shell());
}

blink::WebFrame* Frame::createChildFrame(blink::WebLocalFrame* parent,
                                         blink::WebTreeScopeType scope,
                                         const blink::WebString& frame_name,
                                         blink::WebSandboxFlags sandbox_flags) {
  DCHECK(IsLocal());  // Can't create children of remote frames.
  DCHECK_EQ(parent, web_frame_);
  DCHECK(view_);  // If we're local we have to have a view.
  // Create the view that will house the frame now. We embed once we know the
  // url (see decidePolicyForNavigation()).
  mojo::View* child_view = view_->view_manager()->CreateView();
  child_view->SetVisible(true);
  view_->AddChild(child_view);

  // TODO(sky): the act of creating needs to notify the browser side, not
  // navigation.
  Frame::CreateParams params(frame_tree_manager_, this, child_view->id());
  Frame* child_frame = new Frame(params);
  child_frame->scope_ = scope;

  child_frame->SetView(child_view);

  blink::WebLocalFrame* child_web_frame =
      blink::WebLocalFrame::create(scope, child_frame);
  child_frame->web_frame_ = child_web_frame;
  parent->appendChild(child_web_frame);
  return child_web_frame;
}

void Frame::frameDetached(blink::WebFrame* frame) {
  frameDetached(frame, blink::WebFrameClient::DetachType::Remove);
}

void Frame::frameDetached(blink::WebFrame* web_frame,
                          blink::WebFrameClient::DetachType type) {
  if (type == blink::WebFrameClient::DetachType::Swap) {
    web_frame->close();
    return;
  }

  DCHECK(type == blink::WebFrameClient::DetachType::Remove);
  FrameDetachedImpl(web_frame);
}

blink::WebCookieJar* Frame::cookieJar(blink::WebLocalFrame* frame) {
  // TODO(darin): Blink does not fallback to the Platform provided WebCookieJar.
  // Either it should, as it once did, or we should find another solution here.
  return blink::Platform::current()->cookieJar();
}

blink::WebNavigationPolicy Frame::decidePolicyForNavigation(
    const NavigationPolicyInfo& info) {
  if (parent_ && parent_->IsLocal()) {
    SwapToRemote(info.urlRequest);
    return blink::WebNavigationPolicyIgnore;
  }

  return frame_tree_manager_->DecidePolicyForNavigation(this, info);
}

void Frame::didAddMessageToConsole(const blink::WebConsoleMessage& message,
                                   const blink::WebString& source_name,
                                   unsigned source_line,
                                   const blink::WebString& stack_trace) {
  VLOG(1) << "[" << source_name.utf8() << "(" << source_line << ")] "
          << message.text.utf8();
}

void Frame::didFinishLoad(blink::WebLocalFrame* frame) {
  frame_tree_manager_->OnFrameDidFinishLoad(this);
}

void Frame::didNavigateWithinPage(blink::WebLocalFrame* frame,
                                  const blink::WebHistoryItem& history_item,
                                  blink::WebHistoryCommitType commit_type) {
  frame_tree_manager_->OnFrameDidNavigateLocally(
      this, history_item.urlString().utf8());
}

blink::WebEncryptedMediaClient* Frame::encryptedMediaClient() {
  return global_state()->media_factory()->GetEncryptedMediaClient();
}

void Frame::frameDetached(blink::WebRemoteFrameClient::DetachType type) {
  if (type == blink::WebRemoteFrameClient::DetachType::Swap) {
    web_frame_->close();
    return;
  }

  DCHECK(type == blink::WebRemoteFrameClient::DetachType::Remove);
  FrameDetachedImpl(web_frame_);
}

void Frame::postMessageEvent(blink::WebLocalFrame* source_frame,
                             blink::WebRemoteFrame* target_frame,
                             blink::WebSecurityOrigin target_origin,
                             blink::WebDOMMessageEvent event) {
  NOTIMPLEMENTED();
}

void Frame::initializeChildFrame(const blink::WebRect& frame_rect,
                                 float scale_factor) {
  // TODO(sky): frame_rect is in dips. Need to convert.
  mojo::Rect rect;
  rect.x = frame_rect.x;
  rect.y = frame_rect.y;
  rect.width = frame_rect.width;
  rect.height = frame_rect.height;
  view_->SetBounds(rect);
}

void Frame::navigate(const blink::WebURLRequest& request,
                     bool should_replace_current_entry) {
  NOTIMPLEMENTED();
}

void Frame::reload(bool ignore_cache, bool is_client_redirect) {
  NOTIMPLEMENTED();
}

void Frame::forwardInputEvent(const blink::WebInputEvent* event) {
  NOTIMPLEMENTED();
}

}  // namespace mojo
