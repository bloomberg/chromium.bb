// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_frame.h"

#include <stddef.h>
#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/surfaces/surface_id.h"
#include "components/html_viewer/ax_provider_impl.h"
#include "components/html_viewer/blink_basic_type_converters.h"
#include "components/html_viewer/blink_find_type_converters.h"
#include "components/html_viewer/blink_text_input_type_converters.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/devtools_agent_impl.h"
#include "components/html_viewer/geolocation_client_impl.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_factory.h"
#include "components/html_viewer/html_frame_delegate.h"
#include "components/html_viewer/html_frame_properties.h"
#include "components/html_viewer/html_frame_tree_manager.h"
#include "components/html_viewer/html_widget.h"
#include "components/html_viewer/media_factory.h"
#include "components/html_viewer/stats_collection_controller.h"
#include "components/html_viewer/touch_handler.h"
#include "components/html_viewer/web_layer_tree_view_impl.h"
#include "components/html_viewer/web_storage_namespace_impl.h"
#include "components/html_viewer/web_url_loader_impl.h"
#include "components/mus/public/cpp/scoped_window_ptr.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/ws/ids.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/blink/blink_input_events_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/shell/public/cpp/shell.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "third_party/WebKit/public/web/WebFrameOwnerProperties.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"
#include "third_party/WebKit/public/web/WebRemoteFrameClient.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using mojo::AxProvider;
using mojo::Rect;
using mojo::ServiceProviderPtr;
using mojo::URLResponsePtr;
using web_view::mojom::HTMLMessageEvent;
using web_view::mojom::HTMLMessageEventPtr;

namespace html_viewer {
namespace {

const size_t kMaxTitleChars = 4 * 1024;

web_view::mojom::NavigationTargetType WebNavigationPolicyToNavigationTarget(
    blink::WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyCurrentTab:
      return web_view::mojom::NavigationTargetType::EXISTING_FRAME;
    case blink::WebNavigationPolicyNewBackgroundTab:
    case blink::WebNavigationPolicyNewForegroundTab:
    case blink::WebNavigationPolicyNewWindow:
    case blink::WebNavigationPolicyNewPopup:
      return web_view::mojom::NavigationTargetType::NEW_FRAME;
    default:
      return web_view::mojom::NavigationTargetType::NO_PREFERENCE;
  }
}

HTMLFrame* GetPreviousSibling(HTMLFrame* frame) {
  DCHECK(frame->parent());
  auto iter = std::find(frame->parent()->children().begin(),
                        frame->parent()->children().end(), frame);
  return (iter == frame->parent()->children().begin()) ? nullptr : *(--iter);
}

// See surface_layer.h for a description of this callback.
void SatisfyCallback(cc::SurfaceSequence sequence) {
  // TODO(fsamuel): Implement this.
}

// See surface_layer.h for a description of this callback.
void RequireCallback(cc::SurfaceId surface_id,
                     cc::SurfaceSequence sequence) {
  // TODO(fsamuel): Implement this.
}

}  // namespace

HTMLFrame::HTMLFrame(CreateParams* params)
    : frame_tree_manager_(params->manager),
      parent_(params->parent),
      window_(nullptr),
      id_(params->id),
      web_frame_(nullptr),
      delegate_(params->delegate),
      pending_navigation_(false),
      weak_factory_(this) {
  TRACE_EVENT0("html_viewer", "HTMLFrame::HTMLFrame");
  if (parent_)
    parent_->children_.push_back(this);

  if (params->window && params->window->id() == id_)
    SetWindow(params->window);

  SetReplicatedFrameStateFromClientProperties(params->properties, &state_);

  if (!parent_) {
    CreateRootWebWidget();

    // This is the root of the tree (aka the main frame).
    // Expected order for creating webframes is:
    // . Create local webframe (first webframe must always be local).
    // . Set as main frame on WebView.
    // . Swap to remote (if not local).
    blink::WebLocalFrame* local_web_frame =
        blink::WebLocalFrame::create(state_.tree_scope, this);
    // We need to set the main frame before creating children so that state is
    // properly set up in blink.
    web_view()->setMainFrame(local_web_frame);

    // The resize and setDeviceScaleFactor() needs to be after setting the main
    // frame.
    const gfx::Size size_in_pixels(params->window->bounds().size());
    const gfx::Size size_in_dips = gfx::ConvertSizeToDIP(
        params->window->viewport_metrics().device_pixel_ratio, size_in_pixels);
    web_view()->resize(size_in_dips);
    web_frame_ = local_web_frame;
    web_view()->setDeviceScaleFactor(global_state()->device_pixel_ratio());
    if (id_ != params->window->id()) {
      blink::WebRemoteFrame* remote_web_frame =
          blink::WebRemoteFrame::create(state_.tree_scope, this);
      local_web_frame->swap(remote_web_frame);
      web_frame_ = remote_web_frame;
    } else {
      // Setup a DevTools agent if this is the local main frame and the browser
      // side has set relevant client properties.
      mojo::Array<uint8_t> devtools_id =
          GetValueFromClientProperties("devtools-id", params->properties);
      if (!devtools_id.is_null()) {
        mojo::Array<uint8_t> devtools_state =
            GetValueFromClientProperties("devtools-state", params->properties);
        std::string devtools_state_str = devtools_state.To<std::string>();
        devtools_agent_.reset(new DevToolsAgentImpl(
            web_frame_->toWebLocalFrame(), devtools_id.To<std::string>(),
            devtools_state.is_null() ? nullptr : &devtools_state_str));
      }

      // Collect startup perf data for local main frames in test environments.
      // Child frames aren't tracked, and tracking remote frames is redundant.
      startup_performance_data_collector_ =
          StatsCollectionController::Install(web_frame_, GetShell());
    }
  } else if (!params->is_local_create_child && params->window &&
             id_ == params->window->id()) {
    // Frame represents the local frame, and it isn't the root of the tree.
    HTMLFrame* previous_sibling = GetPreviousSibling(this);
    blink::WebFrame* previous_web_frame =
        previous_sibling ? previous_sibling->web_frame() : nullptr;
    CHECK(!parent_->IsLocal());
    web_frame_ = parent_->web_frame()->toWebRemoteFrame()->createLocalChild(
        state_.tree_scope, state_.name, state_.sandbox_flags, this,
        previous_web_frame,
        // TODO(lazyboy): Replicate WebFrameOwnerProperties where needed.
        blink::WebFrameOwnerProperties());
    CreateLocalRootWebWidget(web_frame_->toWebLocalFrame());
  } else if (!parent_->IsLocal()) {
    web_frame_ = parent_->web_frame()->toWebRemoteFrame()->createRemoteChild(
        state_.tree_scope, state_.name, state_.sandbox_flags, this);
  } else {
    CHECK(params->is_local_create_child);

    blink::WebLocalFrame* child_web_frame =
        blink::WebLocalFrame::create(state_.tree_scope, this);
    web_frame_ = child_web_frame;
    parent_->web_frame_->appendChild(child_web_frame);
  }

  DVLOG(2) << "HTMLFrame init this=" << this << " id=" << id_
           << " local=" << IsLocal()
           << " parent=" << (parent_ ? parent_->id_ : 0u);

  if (!IsLocal()) {
    blink::WebRemoteFrame* remote_web_frame = web_frame_->toWebRemoteFrame();
    if (remote_web_frame) {
      remote_web_frame->setReplicatedOrigin(state_.origin);
      remote_web_frame->setReplicatedName(state_.name);
    }
  }
}

void HTMLFrame::Close() {
  if (GetWebWidget()) {
    // Closing the root widget (WebView) implicitly detaches. For children
    // (which have a WebFrameWidget) a detach() is required. Use a temporary
    // as if 'this' is the root the call to GetWebWidget()->close() deletes
    // 'this'.
    const bool is_child = parent_ != nullptr;
    GetWebWidget()->close();
    if (is_child)
      web_frame_->detach();
  } else {
    web_frame_->detach();
  }
}

const HTMLFrame* HTMLFrame::FindFrame(uint32_t id) const {
  if (id == id_)
    return this;

  for (const HTMLFrame* child : children_) {
    const HTMLFrame* match = child->FindFrame(id);
    if (match)
      return match;
  }
  return nullptr;
}

blink::WebView* HTMLFrame::web_view() {
  blink::WebWidget* web_widget =
      html_widget_ ? html_widget_->GetWidget() : nullptr;
  return web_widget && web_widget->isWebView()
             ? static_cast<blink::WebView*>(web_widget)
             : nullptr;
}

blink::WebWidget* HTMLFrame::GetWebWidget() {
  return html_widget_ ? html_widget_->GetWidget() : nullptr;
}

bool HTMLFrame::IsLocal() const {
  return web_frame_->isWebLocalFrame();
}

bool HTMLFrame::HasLocalDescendant() const {
  if (IsLocal())
    return true;

  for (HTMLFrame* child : children_) {
    if (child->HasLocalDescendant())
      return true;
  }
  return false;
}

void HTMLFrame::LoadRequest(const blink::WebURLRequest& request,
                            base::TimeTicks navigation_start_time) {
  TRACE_EVENT1("html_viewer", "HTMLFrame::LoadRequest",
               "url", request.url().string().utf8());

  DCHECK(IsLocal());

  DVLOG(2) << "HTMLFrame::LoadRequest this=" << this << " id=" << id_
           << " URL=" << GURL(request.url());

  pending_navigation_ = false;
  navigation_start_time_ = navigation_start_time;
  web_frame_->toWebLocalFrame()->loadRequest(request);
}

HTMLFrame::~HTMLFrame() {
  DVLOG(2) << "~HTMLFrame this=" << this << " id=" << id_;

  DCHECK(children_.empty());

  if (parent_) {
    auto iter =
        std::find(parent_->children_.begin(), parent_->children_.end(), this);
    parent_->children_.erase(iter);
  }
  parent_ = nullptr;

  frame_tree_manager_->OnFrameDestroyed(this);

  if (delegate_)
    delegate_->OnFrameDestroyed();

  if (window_) {
    window_->RemoveObserver(this);
    mus::ScopedWindowPtr::DeleteWindowOrWindowManager(window_);
  }
}

blink::WebMediaPlayer* HTMLFrame::createMediaPlayer(
    blink::WebLocalFrame* frame,
    blink::WebMediaPlayer::LoadType load_type,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    blink::WebContentDecryptionModule* initial_cdm,
    const blink::WebString& sink_id,
    blink::WebMediaSession* media_session) {
  return global_state()->media_factory()->CreateMediaPlayer(
      frame, url, client, encrypted_client, initial_cdm, GetShell());
}

blink::WebFrame* HTMLFrame::createChildFrame(
    blink::WebLocalFrame* parent,
    blink::WebTreeScopeType scope,
    const blink::WebString& frame_name,
    blink::WebSandboxFlags sandbox_flags,
    const blink::WebFrameOwnerProperties& frame_owner_properties) {
  DCHECK(IsLocal());  // Can't create children of remote frames.
  DCHECK_EQ(parent, web_frame_);
  DCHECK(window_);  // If we're local we have to have a window.
  // Create the window that will house the frame now. We embed once we know the
  // url (see decidePolicyForNavigation()).
  mus::Window* child_window = window_->connection()->NewWindow();
  ReplicatedFrameState child_state;
  child_state.name = frame_name;
  child_state.tree_scope = scope;
  child_state.sandbox_flags = sandbox_flags;
  mojo::Map<mojo::String, mojo::Array<uint8_t>> client_properties;
  client_properties.mark_non_null();
  ClientPropertiesFromReplicatedFrameState(child_state, &client_properties);

  child_window->SetVisible(true);
  window_->AddChild(child_window);

  HTMLFrame::CreateParams params(frame_tree_manager_, this, child_window->id(),
                                 child_window, client_properties, nullptr);
  params.is_local_create_child = true;
  HTMLFrame* child_frame = GetFirstAncestorWithDelegate()
                               ->delegate_->GetHTMLFactory()
                               ->CreateHTMLFrame(&params);
  child_frame->owned_window_.reset(new mus::ScopedWindowPtr(child_window));

  web_view::mojom::FrameClientPtr client_ptr;
  child_frame->frame_client_binding_.reset(
      new mojo::Binding<web_view::mojom::FrameClient>(
          child_frame, mojo::GetProxy(&client_ptr)));
  server_->OnCreatedFrame(GetProxy(&(child_frame->server_)),
                          std::move(client_ptr), child_window->id(),
                          std::move(client_properties));
  return child_frame->web_frame_;
}

void HTMLFrame::frameDetached(blink::WebFrame* web_frame,
                              blink::WebFrameClient::DetachType type) {
  if (type == blink::WebFrameClient::DetachType::Swap) {
    web_frame->close();
    return;
  }

  DCHECK(type == blink::WebFrameClient::DetachType::Remove);
  FrameDetachedImpl(web_frame);
}

blink::WebCookieJar* HTMLFrame::cookieJar(blink::WebLocalFrame* frame) {
  // TODO(darin): Blink does not fallback to the Platform provided WebCookieJar.
  // Either it should, as it once did, or we should find another solution here.
  return blink::Platform::current()->cookieJar();
}

blink::WebNavigationPolicy HTMLFrame::decidePolicyForNavigation(
    const NavigationPolicyInfo& info) {
  // If we have extraData() it means we already have the url response
  // (presumably because we are being called via Navigate()). In that case we
  // can go ahead and navigate locally.
  if (info.urlRequest.extraData()) {
    DCHECK_EQ(blink::WebNavigationPolicyCurrentTab, info.defaultPolicy);
    return blink::WebNavigationPolicyCurrentTab;
  }

  // about:blank is treated as the same origin and is always allowed for frames.
  if (parent_ && info.urlRequest.url() == GURL(url::kAboutBlankURL) &&
      info.defaultPolicy == blink::WebNavigationPolicyCurrentTab) {
    return blink::WebNavigationPolicyCurrentTab;
  }

  // Ask the Frame to handle the navigation. Returning
  // WebNavigationPolicyHandledByClient to inform blink that the navigation is
  // being handled.
  DVLOG(2) << "HTMLFrame::decidePolicyForNavigation calls "
           << "Frame::RequestNavigate this=" << this << " id=" << id_
           << " URL=" << GURL(info.urlRequest.url());

  mojo::URLRequestPtr url_request = mojo::URLRequest::From(info.urlRequest);
  url_request->originating_time_ticks =
      base::TimeTicks::Now().ToInternalValue();
  server_->RequestNavigate(
      WebNavigationPolicyToNavigationTarget(info.defaultPolicy), id_,
      std::move(url_request));

  // TODO(yzshen): crbug.com/532556 If the server side drops the request,
  // this frame will be in permenant-loading state. We should send a
  // notification to mark this frame as not loading in that case. We also need
  // to better keep track of multiple pending navigations.
  pending_navigation_ = true;
  return blink::WebNavigationPolicyHandledByClient;
}

bool HTMLFrame::hasPendingNavigation(blink::WebLocalFrame* frame) {
  return pending_navigation_;
}

void HTMLFrame::didHandleOnloadEvents(blink::WebLocalFrame* frame) {
  DVLOG(2) << "XXX HTMLFrame::didHandleOnloadEvents id=" << id_;
  static bool recorded = false;
  if (!recorded && startup_performance_data_collector_) {
    startup_performance_data_collector_->SetFirstWebContentsMainFrameLoadTicks(
        base::TimeTicks::Now().ToInternalValue());
    recorded = true;
  }
}

void HTMLFrame::didAddMessageToConsole(const blink::WebConsoleMessage& message,
                                       const blink::WebString& source_name,
                                       unsigned source_line,
                                       const blink::WebString& stack_trace) {
  VLOG(1) << "[" << source_name.utf8() << "(" << source_line << ")] "
          << message.text.utf8();
}

void HTMLFrame::didFinishLoad(blink::WebLocalFrame* frame) {
  if (GetFirstAncestorWithDelegate() == this)
    delegate_->OnFrameDidFinishLoad();
}

void HTMLFrame::didNavigateWithinPage(blink::WebLocalFrame* frame,
                                      const blink::WebHistoryItem& history_item,
                                      blink::WebHistoryCommitType commit_type) {
  server_->DidNavigateLocally(history_item.urlString().utf8());
}

blink::WebGeolocationClient* HTMLFrame::geolocationClient() {
  if (!geolocation_client_impl_)
    geolocation_client_impl_.reset(new GeolocationClientImpl);
  return geolocation_client_impl_.get();
}

blink::WebEncryptedMediaClient* HTMLFrame::encryptedMediaClient() {
  return global_state()->media_factory()->GetEncryptedMediaClient();
}

void HTMLFrame::didStartLoading(bool to_different_document) {
  server_->LoadingStateChanged(true, 0.0);
}

void HTMLFrame::didStopLoading() {
  server_->LoadingStateChanged(false, 1.0);
}

void HTMLFrame::didChangeLoadProgress(double load_progress) {
  server_->LoadingStateChanged(true, load_progress);
}

void HTMLFrame::dispatchLoad() {
  // According to comments of WebFrameClient::dispatchLoad(), this should only
  // be called when the parent frame is remote.
  DCHECK(parent_ && !parent_->IsLocal());
  server_->DispatchLoadEventToParent();
}

void HTMLFrame::didChangeName(blink::WebLocalFrame* frame,
                              const blink::WebString& name) {
  state_.name = name;
  server_->SetClientProperty(kPropertyFrameName,
                             FrameNameToClientProperty(name));
}

void HTMLFrame::didCommitProvisionalLoad(
    blink::WebLocalFrame* frame,
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type) {
  state_.origin = FrameOrigin(frame);
  server_->SetClientProperty(kPropertyFrameOrigin,
                             FrameOriginToClientProperty(frame));

  // TODO(erg): We need to pass way more information from here through to the
  // other side. See FrameHostMsg_DidCommitProvisionalLoad_Params. It is a grab
  // bag of everything and it looks like a combination of
  // NavigatorImpl::DidNavigate and
  // NavigationControllerImpl::RendererDidNavigate use everything passed
  // through.
  server_->DidCommitProvisionalLoad();

  if (!navigation_start_time_.is_null()) {
    frame->dataSource()->setNavigationStartTime(
        navigation_start_time_.ToInternalValue() /
        static_cast<double>(base::Time::kMicrosecondsPerSecond));
    navigation_start_time_ = base::TimeTicks();
  }
}

void HTMLFrame::didReceiveTitle(blink::WebLocalFrame* frame,
                                const blink::WebString& title,
                                blink::WebTextDirection direction) {
  // TODO(beng): handle |direction|.
  mojo::String formatted;
  if (!title.isNull()) {
    formatted =
        mojo::String::From(base::string16(title).substr(0, kMaxTitleChars));
  }
  server_->TitleChanged(formatted);
}

void HTMLFrame::reportFindInFrameMatchCount(int identifier,
                                            int count,
                                            bool finalUpdate) {
  server_->OnFindInFrameCountUpdated(identifier, count, finalUpdate);
}

void HTMLFrame::reportFindInPageSelection(int identifier,
                                          int activeMatchOrdinal,
                                          const blink::WebRect& selection) {
  server_->OnFindInPageSelectionUpdated(identifier, activeMatchOrdinal);
}

bool HTMLFrame::shouldSearchSingleFrame() {
  return true;
}

void HTMLFrame::Bind(
    web_view::mojom::FramePtr frame,
    mojo::InterfaceRequest<web_view::mojom::FrameClient> frame_client_request) {
  DCHECK(IsLocal());
  server_ = std::move(frame);
  server_.set_connection_error_handler(
      base::Bind(&HTMLFrame::Close, base::Unretained(this)));
  frame_client_binding_.reset(new mojo::Binding<web_view::mojom::FrameClient>(
      this, std::move(frame_client_request)));
}

void HTMLFrame::SetValueFromClientProperty(const std::string& name,
                                           mojo::Array<uint8_t> new_data) {
  if (IsLocal())
    return;

  // Only the name and origin dynamically change.
  if (name == kPropertyFrameOrigin) {
    state_.origin = FrameOriginFromClientProperty(new_data);
    web_frame_->toWebRemoteFrame()->setReplicatedOrigin(state_.origin);
  } else if (name == kPropertyFrameName) {
    state_.name = FrameNameFromClientProperty(new_data);
    web_frame_->toWebRemoteFrame()->setReplicatedName(state_.name);
  }
}

HTMLFrame* HTMLFrame::GetFirstAncestorWithDelegate() {
  HTMLFrame* frame = this;
  while (frame && !frame->delegate_)
    frame = frame->parent_;
  return frame;
}

mojo::Shell* HTMLFrame::GetShell() {
  return GetFirstAncestorWithDelegate()->delegate_->GetShell();
}

web_view::mojom::Frame* HTMLFrame::GetServerFrame() {
  // Prefer an ancestor with a server Frame.
  for (HTMLFrame* frame = this; frame; frame = frame->parent_) {
    if (frame->server_.get())
      return frame->server_.get();
  }

  // We're a remote frame with no local frame ancestors. Use the server Frame
  // from the local frame of the HTMLFrameTreeManager.
  return frame_tree_manager_->local_frame_->server_.get();
}

void HTMLFrame::SetWindow(mus::Window* window) {
  if (window_) {
    window_->set_input_event_handler(nullptr);
    window_->RemoveObserver(this);
  }
  window_ = window;
  if (window_) {
    window_->AddObserver(this);
    window_->set_input_event_handler(this);
  }
}

void HTMLFrame::CreateRootWebWidget() {
  DCHECK(!html_widget_);
  if (window_) {
    HTMLWidgetRootLocal::CreateParams create_params(GetShell(), global_state(),
                                                    window_);
    html_widget_.reset(
        delegate_->GetHTMLFactory()->CreateHTMLWidgetRootLocal(&create_params));
  } else {
    html_widget_.reset(new HTMLWidgetRootRemote(global_state()));
  }
}

void HTMLFrame::CreateLocalRootWebWidget(blink::WebLocalFrame* local_frame) {
  DCHECK(!html_widget_);
  DCHECK(IsLocal());
  html_widget_.reset(new HTMLWidgetLocalRoot(GetShell(), global_state(),
                                             window_, local_frame));
}

void HTMLFrame::UpdateFocus() {
  blink::WebWidget* web_widget = GetWebWidget();
  if (!web_widget || !window_)
    return;
  const bool is_focused = window_ && window_->HasFocus();
  web_widget->setFocus(is_focused);
  if (web_widget->isWebView())
    static_cast<blink::WebView*>(web_widget)->setIsActive(is_focused);
}

void HTMLFrame::SwapToRemote() {
  TRACE_EVENT0("html_viewer", "HTMLFrame::SwapToRemote");

  DVLOG(2) << "HTMLFrame::SwapToRemote this=" << this << " id=" << id_;

  DCHECK(IsLocal());

  HTMLFrameDelegate* delegate = delegate_;
  delegate_ = nullptr;

  blink::WebRemoteFrame* remote_frame =
      blink::WebRemoteFrame::create(state_.tree_scope, this);
  remote_frame->initializeFromFrame(web_frame_->toWebLocalFrame());
  // swap() ends up calling us back and we then close the frame, which deletes
  // it.
  web_frame_->swap(remote_frame);
  if (owned_window_) {
    surface_layer_ =
          cc::SurfaceLayer::Create(cc_blink::WebLayerImpl::LayerSettings(),
                                   base::Bind(&SatisfyCallback),
                                   base::Bind(&RequireCallback));
    surface_layer_->SetSurfaceId(cc::SurfaceId(owned_window_->window()->id()),
                                 global_state()->device_pixel_ratio(),
                                 owned_window_->window()->bounds().size());

    web_layer_.reset(new cc_blink::WebLayerImpl(surface_layer_));
  }
  remote_frame->setRemoteWebLayer(web_layer_.get());
  remote_frame->setReplicatedName(state_.name);
  remote_frame->setReplicatedOrigin(state_.origin);
  remote_frame->setReplicatedSandboxFlags(state_.sandbox_flags);

  // Tell the frame that it is actually loading. This prevents its parent
  // from prematurely dispatching load event.
  remote_frame->didStartLoading();
  pending_navigation_ = false;

  web_frame_ = remote_frame;
  SetWindow(nullptr);
  server_.reset();
  frame_client_binding_.reset();
  if (delegate)
    delegate->OnFrameSwappedToRemote();
}

void HTMLFrame::SwapToLocal(
    HTMLFrameDelegate* delegate,
    mus::Window* window,
    const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties) {
  TRACE_EVENT0("html_viewer", "HTMLFrame::SwapToLocal");
  DVLOG(2) << "HTMLFrame::SwapToLocal this=" << this << " id=" << id_;
  CHECK(!IsLocal());
  // It doesn't make sense for the root to swap to local.
  CHECK(parent_);
  delegate_ = delegate;
  SetWindow(window);
  SetReplicatedFrameStateFromClientProperties(properties, &state_);
  blink::WebLocalFrame* local_web_frame =
      blink::WebLocalFrame::createProvisional(
          this, web_frame_->toWebRemoteFrame(), state_.sandbox_flags,
          // TODO(lazyboy): Figure out replicating WebFrameOwnerProperties.
          blink::WebFrameOwnerProperties());
  // The swap() ends up calling to frameDetached() and deleting the old.
  web_frame_->swap(local_web_frame);
  web_frame_ = local_web_frame;

  web_layer_.reset();
}

void HTMLFrame::SwapDelegate(HTMLFrameDelegate* delegate) {
  DCHECK(IsLocal());
  HTMLFrameDelegate* old_delegate = delegate_;
  delegate_ = delegate;
  delegate->OnSwap(this, old_delegate);
}

blink::WebElement HTMLFrame::GetFocusedElement() {
  if (!web_view())
    return blink::WebElement();

  HTMLFrame* frame = this;
  while (frame) {
    if (frame->web_view()) {
      if (frame->web_view()->focusedFrame() == web_frame_) {
        blink::WebDocument doc = web_frame_->document();
        if (!doc.isNull())
          return doc.focusedElement();
      }
      return blink::WebElement();
    }
    frame = frame->parent();
  }

  return blink::WebElement();
}

HTMLFrame* HTMLFrame::FindFrameWithWebFrame(blink::WebFrame* web_frame) {
  if (web_frame_ == web_frame)
    return this;
  for (HTMLFrame* child_frame : children_) {
    HTMLFrame* result = child_frame->FindFrameWithWebFrame(web_frame);
    if (result)
      return result;
  }
  return nullptr;
}

void HTMLFrame::FrameDetachedImpl(blink::WebFrame* web_frame) {
  DCHECK_EQ(web_frame_, web_frame);

  while (!children_.empty()) {
    HTMLFrame* child = children_.front();
    child->Close();
    DCHECK(children_.empty() || children_.front() != child);
  }

  if (web_frame->parent())
    web_frame->parent()->removeChild(web_frame);

  delete this;
}

void HTMLFrame::OnWindowBoundsChanged(mus::Window* window,
                                      const gfx::Rect& old_bounds,
                                      const gfx::Rect& new_bounds) {
  DCHECK_EQ(window, window_);
  if (html_widget_)
    html_widget_->OnWindowBoundsChanged(window);
}

void HTMLFrame::OnWindowDestroyed(mus::Window* window) {
  DCHECK_EQ(window, window_);
  window_->RemoveObserver(this);
  window_ = nullptr;
  Close();
}

void HTMLFrame::OnWindowFocusChanged(mus::Window* gained_focus,
                                     mus::Window* lost_focus) {
  UpdateFocus();
}

void HTMLFrame::OnWindowInputEvent(mus::Window* window,
                                   mus::mojom::EventPtr event,
                                   scoped_ptr<base::Closure>* ack_callback) {
  if (event->pointer_data && event->pointer_data->location) {
    // Blink expects coordintes to be in DIPs.
    event->pointer_data->location->x /= global_state()->device_pixel_ratio();
    event->pointer_data->location->y /= global_state()->device_pixel_ratio();
    event->pointer_data->location->screen_x /=
        global_state()->device_pixel_ratio();
    event->pointer_data->location->screen_y /=
        global_state()->device_pixel_ratio();
  }

  blink::WebWidget* web_widget = GetWebWidget();

  if (!touch_handler_ && web_widget)
    touch_handler_.reset(new TouchHandler(web_widget));

  if (touch_handler_ &&
      (event->action == mus::mojom::EventType::POINTER_DOWN ||
       event->action == mus::mojom::EventType::POINTER_UP ||
       event->action == mus::mojom::EventType::POINTER_CANCEL ||
       event->action == mus::mojom::EventType::POINTER_MOVE) &&
      event->pointer_data &&
      event->pointer_data->kind == mus::mojom::PointerKind::TOUCH) {
    touch_handler_->OnTouchEvent(*event);
    return;
  }

  if (!web_widget)
    return;

  scoped_ptr<blink::WebInputEvent> web_event =
      event.To<scoped_ptr<blink::WebInputEvent>>();
  if (web_event)
    web_widget->handleInputEvent(*web_event);
}

void HTMLFrame::OnConnect(
    web_view::mojom::FramePtr frame,
    uint32_t change_id,
    uint32_t window_id,
    web_view::mojom::WindowConnectType window_connect_type,
    mojo::Array<web_view::mojom::FrameDataPtr> frame_data,
    int64_t navigation_start_time_ticks,
    const OnConnectCallback& callback) {
  // This is called if this frame is created by way of OnCreatedFrame().
  callback.Run();
}

void HTMLFrame::OnFrameAdded(uint32_t change_id,
                             web_view::mojom::FrameDataPtr frame_data) {
  frame_tree_manager_->ProcessOnFrameAdded(this, change_id,
                                           std::move(frame_data));
}

void HTMLFrame::OnFrameRemoved(uint32_t change_id, uint32_t frame_id) {
  frame_tree_manager_->ProcessOnFrameRemoved(this, change_id, frame_id);
}

void HTMLFrame::OnFrameClientPropertyChanged(uint32_t frame_id,
                                             const mojo::String& name,
                                             mojo::Array<uint8_t> new_value) {
  frame_tree_manager_->ProcessOnFrameClientPropertyChanged(
      this, frame_id, name, std::move(new_value));
}

void HTMLFrame::OnPostMessageEvent(uint32_t source_frame_id,
                                   uint32_t target_frame_id,
                                   HTMLMessageEventPtr serialized_event) {
  NOTIMPLEMENTED();  // For message ports.

  HTMLFrame* target = frame_tree_manager_->root_->FindFrame(target_frame_id);
  HTMLFrame* source = frame_tree_manager_->root_->FindFrame(source_frame_id);
  if (!target || !source) {
    DVLOG(1) << "Invalid source or target for PostMessage";
    return;
  }

  if (!target->IsLocal()) {
    DVLOG(1) << "Target for PostMessage is not lot local";
    return;
  }

  blink::WebLocalFrame* target_web_frame =
      target->web_frame_->toWebLocalFrame();

  blink::WebSerializedScriptValue serialized_script_value;
  serialized_script_value = blink::WebSerializedScriptValue::fromString(
      serialized_event->data.To<blink::WebString>());

  // We must pass in the target_origin to do the security check on this side,
  // since it may have changed since the original postMessage call was made.
  blink::WebSecurityOrigin target_origin;
  if (!serialized_event->target_origin.is_null()) {
    target_origin = blink::WebSecurityOrigin::createFromString(
        serialized_event->target_origin.To<blink::WebString>());
  }

  // TODO(esprehn): Shouldn't this also fill in channels like RenderFrameImpl?
  blink::WebMessagePortChannelArray channels;
  blink::WebDOMMessageEvent msg_event(serialized_script_value,
      serialized_event->source_origin.To<blink::WebString>(),
      source->web_frame_, target_web_frame->document(), channels);

  target_web_frame->dispatchMessageEventWithOriginCheck(target_origin,
                                                        msg_event);
}

void HTMLFrame::OnWillNavigate(const mojo::String& origin,
                               const OnWillNavigateCallback& callback) {
  bool should_swap = true;

  if (this == frame_tree_manager_->local_frame_) {
    HTMLFrame* new_local_frame = frame_tree_manager_->FindNewLocalFrame();
    if (!new_local_frame) {
      // All local frames are descendants of |this|. In this case, the whole
      // frame tree in the current process is going to be deleted very soon. We
      // don't have to swap.
      should_swap = false;
    } else {
      frame_tree_manager_->local_frame_ = new_local_frame;
    }
  }

  DVLOG(2) << "HTMLFrame::OnWillNavigate this=" << this << " id=" << id_
           << " local=" << IsLocal() << " should_swap=" << should_swap;
  callback.Run();
  if (should_swap) {
    SwapToRemote();
    const blink::WebSecurityOrigin security_origin(
        blink::WebSecurityOrigin::createFromString(
            blink::WebString::fromUTF8(origin)));
    web_frame_->toWebRemoteFrame()->setReplicatedOrigin(security_origin);
  }
}

void HTMLFrame::OnFrameLoadingStateChanged(uint32_t frame_id, bool loading) {
  HTMLFrame* frame = frame_tree_manager_->root_->FindFrame(frame_id);
  // TODO(yzshen): (Apply to this method and the one below.) Is it possible that
  // at this point the frame is already hosting a different document?
  if (frame && !frame->IsLocal()) {
    if (loading)
      frame->web_frame_->toWebRemoteFrame()->didStartLoading();
    else
      frame->web_frame_->toWebRemoteFrame()->didStopLoading();
  }
}

void HTMLFrame::OnDispatchFrameLoadEvent(uint32_t frame_id) {
  HTMLFrame* frame = frame_tree_manager_->root_->FindFrame(frame_id);
  if (frame && !frame->IsLocal())
    frame->web_frame_->toWebRemoteFrame()->DispatchLoadEventForFrameOwner();
}

void HTMLFrame::Find(int32_t request_id,
                     const mojo::String& search_text,
                     web_view::mojom::FindOptionsPtr options,
                     bool wrap_within_frame,
                     const FindCallback& callback) {
  blink::WebRect selection_rect;
  bool result = web_frame_->toWebLocalFrame()->find(
      request_id, search_text.To<blink::WebString>(),
      options.To<blink::WebFindOptions>(), wrap_within_frame, &selection_rect);

  if (!result) {
    // don't leave text selected as you move to the next frame.
    web_frame_->executeCommand(blink::WebString::fromUTF8("Unselect"),
                               GetFocusedElement());
  }

  callback.Run(result);
}

void HTMLFrame::StopFinding(bool clear_selection) {
  // TODO(erg): |clear_selection| isn't correct; this should be a state enum
  // that lets us STOP_FIND_ACTION_ACTIVATE_SELECTION, too.
  if (clear_selection) {
    blink::WebElement focused_element = GetFocusedElement();
    if (!focused_element.isNull()) {
      web_frame_->executeCommand(blink::WebString::fromUTF8("Unselect"),
                                 focused_element);
    }
  }

  web_frame_->toWebLocalFrame()->stopFinding(clear_selection);
}

void HTMLFrame::HighlightFindResults(int32_t request_id,
                                     const mojo::String& search_text,
                                     web_view::mojom::FindOptionsPtr options,
                                     bool reset) {
  web_frame_->toWebLocalFrame()->scopeStringMatches(
      request_id, search_text.To<blink::WebString>(),
      options.To<blink::WebFindOptions>(), reset);
}

void HTMLFrame::StopHighlightingFindResults() {
  web_frame_->toWebLocalFrame()->resetMatchCount();
  web_frame_->toWebLocalFrame()->cancelPendingScopingEffort();
}

void HTMLFrame::frameDetached(blink::WebRemoteFrameClient::DetachType type) {
  if (type == blink::WebRemoteFrameClient::DetachType::Swap) {
    web_frame_->close();
    return;
  }

  DCHECK(type == blink::WebRemoteFrameClient::DetachType::Remove);
  FrameDetachedImpl(web_frame_);
}

void HTMLFrame::postMessageEvent(blink::WebLocalFrame* source_web_frame,
                                 blink::WebRemoteFrame* target_web_frame,
                                 blink::WebSecurityOrigin target_origin,
                                 blink::WebDOMMessageEvent web_event) {
  NOTIMPLEMENTED();  // message_ports aren't implemented yet.

  HTMLFrame* source_frame =
      frame_tree_manager_->root_->FindFrameWithWebFrame(source_web_frame);
  DCHECK(source_frame);
  HTMLFrame* target_frame =
      frame_tree_manager_->root_->FindFrameWithWebFrame(target_web_frame);
  DCHECK(target_frame);

  HTMLMessageEventPtr event(HTMLMessageEvent::New());
  event->data = mojo::Array<uint8_t>::From(web_event.data().toString());
  event->source_origin = mojo::String::From(web_event.origin());
  if (!target_origin.isNull())
    event->target_origin = mojo::String::From(target_origin.toString());

  source_frame->server_->PostMessageEventToFrame(target_frame->id_,
                                                 std::move(event));
}

void HTMLFrame::initializeChildFrame(const blink::WebRect& frame_rect,
                                     float scale_factor) {
  // NOTE: |scale_factor| is always 1.
  const gfx::Rect rect_in_dip(frame_rect.x, frame_rect.y, frame_rect.width,
                              frame_rect.height);
  const gfx::Rect rect_in_pixels(gfx::ConvertRectToPixel(
      global_state()->device_pixel_ratio(), rect_in_dip));
  window_->SetBounds(rect_in_pixels);
}

void HTMLFrame::navigate(const blink::WebURLRequest& request,
                         bool should_replace_current_entry) {
  // TODO: support |should_replace_current_entry|.
  NOTIMPLEMENTED();  // for |should_replace_current_entry
  mojo::URLRequestPtr url_request = mojo::URLRequest::From(request);
  GetServerFrame()->RequestNavigate(
      web_view::mojom::NavigationTargetType::EXISTING_FRAME, id_,
      std::move(url_request));
}

void HTMLFrame::reload(bool ignore_cache, bool is_client_redirect) {
  NOTIMPLEMENTED();
}

void HTMLFrame::frameRectsChanged(const blink::WebRect& frame_rect) {
  // Only the owner of window can update its size.
  if (!owned_window_)
    return;

  const gfx::Rect rect_in_dip(frame_rect.x, frame_rect.y, frame_rect.width,
                              frame_rect.height);
  const gfx::Rect rect_in_pixels(gfx::ConvertRectToPixel(
      global_state()->device_pixel_ratio(), rect_in_dip));
  owned_window_->window()->SetBounds(rect_in_pixels);

  if (!surface_layer_)
    return;

  surface_layer_->SetSurfaceId(cc::SurfaceId(owned_window_->window()->id()),
                               global_state()->device_pixel_ratio(),
                               owned_window_->window()->bounds().size());
}

}  // namespace mojo
