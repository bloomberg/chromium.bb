// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_frame.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "components/html_viewer/ax_provider_impl.h"
#include "components/html_viewer/blink_basic_type_converters.h"
#include "components/html_viewer/blink_input_events_type_converters.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/geolocation_client_impl.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_frame_delegate.h"
#include "components/html_viewer/html_frame_properties.h"
#include "components/html_viewer/html_frame_tree_manager.h"
#include "components/html_viewer/media_factory.h"
#include "components/html_viewer/touch_handler.h"
#include "components/html_viewer/web_layer_impl.h"
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
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
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
#include "url/origin.h"

using mojo::AxProvider;
using mojo::Rect;
using mojo::ServiceProviderPtr;
using mojo::URLResponsePtr;
using mojo::View;
using mojo::WeakBindToRequest;

namespace mojo {

#define TEXT_INPUT_TYPE_ASSERT(NAME, Name) \
  COMPILE_ASSERT(static_cast<int32>(TEXT_INPUT_TYPE_##NAME) == \
                 static_cast<int32>(blink::WebTextInputType##Name), \
                 text_input_type_should_match)
TEXT_INPUT_TYPE_ASSERT(NONE, None);
TEXT_INPUT_TYPE_ASSERT(TEXT, Text);
TEXT_INPUT_TYPE_ASSERT(PASSWORD, Password);
TEXT_INPUT_TYPE_ASSERT(SEARCH, Search);
TEXT_INPUT_TYPE_ASSERT(EMAIL, Email);
TEXT_INPUT_TYPE_ASSERT(NUMBER, Number);
TEXT_INPUT_TYPE_ASSERT(TELEPHONE, Telephone);
TEXT_INPUT_TYPE_ASSERT(URL, URL);
TEXT_INPUT_TYPE_ASSERT(DATE, Date);
TEXT_INPUT_TYPE_ASSERT(DATE_TIME, DateTime);
TEXT_INPUT_TYPE_ASSERT(DATE_TIME_LOCAL, DateTimeLocal);
TEXT_INPUT_TYPE_ASSERT(MONTH, Month);
TEXT_INPUT_TYPE_ASSERT(TIME, Time);
TEXT_INPUT_TYPE_ASSERT(WEEK, Week);
TEXT_INPUT_TYPE_ASSERT(TEXT_AREA, TextArea);

#define TEXT_INPUT_FLAG_ASSERT(NAME, Name) \
  COMPILE_ASSERT(static_cast<int32>(TEXT_INPUT_FLAG_##NAME) == \
                 static_cast<int32>(blink::WebTextInputFlag##Name), \
                 text_input_flag_should_match)
TEXT_INPUT_FLAG_ASSERT(NONE, None);
TEXT_INPUT_FLAG_ASSERT(AUTO_COMPLETE_ON, AutocompleteOn);
TEXT_INPUT_FLAG_ASSERT(AUTO_COMPLETE_OFF, AutocompleteOff);
TEXT_INPUT_FLAG_ASSERT(AUTO_CORRECT_ON, AutocorrectOn);
TEXT_INPUT_FLAG_ASSERT(AUTO_CORRECT_OFF, AutocorrectOff);
TEXT_INPUT_FLAG_ASSERT(SPELL_CHECK_ON, SpellcheckOn);
TEXT_INPUT_FLAG_ASSERT(SPELL_CHECK_OFF, SpellcheckOff);
TEXT_INPUT_FLAG_ASSERT(AUTO_CAPITALIZE_NONE, AutocapitalizeNone);
TEXT_INPUT_FLAG_ASSERT(AUTO_CAPITALIZE_CHARACTERS, AutocapitalizeCharacters);
TEXT_INPUT_FLAG_ASSERT(AUTO_CAPITALIZE_WORDS, AutocapitalizeWords);
TEXT_INPUT_FLAG_ASSERT(AUTO_CAPITALIZE_SENTENCES, AutocapitalizeSentences);

template <>
struct TypeConverter<TextInputType, blink::WebTextInputType> {
  static TextInputType Convert(const blink::WebTextInputType& input) {
    return static_cast<TextInputType>(input);
  }
};

}  // namespace mojo

namespace html_viewer {
namespace {

mandoline::NavigationTarget WebNavigationPolicyToNavigationTarget(
    blink::WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyCurrentTab:
      return mandoline::NAVIGATION_TARGET_SOURCE_NODE;
    case blink::WebNavigationPolicyNewBackgroundTab:
    case blink::WebNavigationPolicyNewForegroundTab:
    case blink::WebNavigationPolicyNewWindow:
    case blink::WebNavigationPolicyNewPopup:
      return mandoline::NAVIGATION_TARGET_NEW_NODE;
    default:
      return mandoline::NAVIGATION_TARGET_DEFAULT;
  }
}

void ConfigureSettings(blink::WebSettings* settings) {
  settings->setCookieEnabled(true);
  settings->setDefaultFixedFontSize(13);
  settings->setDefaultFontSize(16);
  settings->setLoadsImagesAutomatically(true);
  settings->setJavaScriptEnabled(true);
}

HTMLFrame* GetPreviousSibling(HTMLFrame* frame) {
  DCHECK(frame->parent());
  auto iter = std::find(frame->parent()->children().begin(),
                        frame->parent()->children().end(), frame);
  return (iter == frame->parent()->children().begin()) ? nullptr : *(--iter);
}

bool CanNavigateLocally(blink::WebFrame* frame,
                        const blink::WebURLRequest& request) {
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

HTMLFrame::HTMLFrame(const HTMLFrame::CreateParams& params)
    : frame_tree_manager_(params.manager),
      parent_(params.parent),
      view_(nullptr),
      id_(params.id),
      web_frame_(nullptr),
      web_widget_(nullptr),
      scope_(blink::WebTreeScopeType::Document),
      delegate_(nullptr),
      weak_factory_(this) {
  if (parent_)
    parent_->children_.push_back(this);
}

void HTMLFrame::Init(
    mojo::View* local_view,
    const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties) {
  if (local_view && local_view->id() == id_)
    SetView(local_view);

  // Ignore return value and assume Document scope if scope isn't known.
  FrameTreeScopeFromClientProperty(
      GetValueFromClientProperties(kPropertyFrameTreeScope, properties),
      &scope_);

  blink::WebSandboxFlags sandbox_flags = blink::WebSandboxFlags::None;
  FrameSandboxFlagsFromClientProperty(
      GetValueFromClientProperties(kPropertyFrameSandboxFlags, properties),
      &sandbox_flags);

  if (!parent_) {
    CreateRootWebWidget();
    // This is the root of the tree (aka the main frame).
    // Expected order for creating webframes is:
    // . Create local webframe (first webframe must always be local).
    // . Set as main frame on WebView.
    // . Swap to remote (if not local).
    blink::WebLocalFrame* local_web_frame =
        blink::WebLocalFrame::create(scope_, this);
    // We need to set the main frame before creating children so that state is
    // properly set up in blink.
    web_view()->setMainFrame(local_web_frame);
    const gfx::Size size_in_pixels(local_view->bounds().width,
                                   local_view->bounds().height);
    const gfx::Size size_in_dips = gfx::ConvertSizeToDIP(
        local_view->viewport_metrics().device_pixel_ratio, size_in_pixels);
    web_widget_->resize(size_in_dips);
    web_frame_ = local_web_frame;
    web_view()->setDeviceScaleFactor(global_state()->device_pixel_ratio());
    if (id_ != local_view->id()) {
      blink::WebRemoteFrame* remote_web_frame =
          blink::WebRemoteFrame::create(scope_, this);
      local_web_frame->swap(remote_web_frame);
      web_frame_ = remote_web_frame;
    }
  } else if (local_view && id_ == local_view->id()) {
    // Frame represents the local frame, and it isn't the root of the tree.
    HTMLFrame* previous_sibling = GetPreviousSibling(this);
    blink::WebFrame* previous_web_frame =
        previous_sibling ? previous_sibling->web_frame() : nullptr;
    DCHECK(!parent_->IsLocal());
    web_frame_ = parent_->web_frame()->toWebRemoteFrame()->createLocalChild(
        scope_, FrameNameFromClientProperty(GetValueFromClientProperties(
                    kPropertyFrameName, properties)),
        sandbox_flags, this, previous_web_frame);
    CreateLocalRootWebWidget(web_frame_->toWebLocalFrame());
  } else if (!parent_->IsLocal()) {
    web_frame_ = parent_->web_frame()->toWebRemoteFrame()->createRemoteChild(
        scope_, FrameNameFromClientProperty(GetValueFromClientProperties(
                    kPropertyFrameName, properties)),
        sandbox_flags, this);
  } else {
    // This is hit if we're asked to create a child frame of a local parent.
    // This should never happen (if we create a local child we don't call
    // Init(), and the frame server should not being creating child frames of
    // this frame).
    NOTREACHED();
  }

  if (!IsLocal()) {
    blink::WebRemoteFrame* remote_web_frame = web_frame_->toWebRemoteFrame();
    if (remote_web_frame) {
      remote_web_frame->setReplicatedOrigin(FrameOriginFromClientProperty(
          GetValueFromClientProperties(kPropertyFrameOrigin, properties)));
      remote_web_frame->setReplicatedName(FrameNameFromClientProperty(
          GetValueFromClientProperties(kPropertyFrameName, properties)));
    }
  }
}

void HTMLFrame::Close() {
  if (web_widget_) {
    // Closing the root widget (WebView) implicitly detaches. For children
    // (which have a WebFrameWidget) a detach() is required. Use a temporary
    // as if 'this' is the root the call to web_widget_->close() deletes
    // 'this'.
    const bool is_child = parent_ != nullptr;
    web_widget_->close();
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
  return web_widget_ && web_widget_->isWebView()
             ? static_cast<blink::WebView*>(web_widget_)
             : nullptr;
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

HTMLFrame::~HTMLFrame() {
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
    if (view_->view_manager()->GetRoot() == view_)
      delete view_->view_manager();
    else
      view_->Destroy();
  }
}

void HTMLFrame::Bind(mandoline::FrameTreeServerPtr frame_tree_server,
                     mojo::InterfaceRequest<mandoline::FrameTreeClient>
                         frame_tree_client_request) {
  DCHECK(IsLocal());
  // TODO(sky): error handling.
  server_ = frame_tree_server.Pass();
  frame_tree_client_binding_.reset(
      new mojo::Binding<mandoline::FrameTreeClient>(
          this, frame_tree_client_request.Pass()));
}

void HTMLFrame::SetValueFromClientProperty(const std::string& name,
                                           mojo::Array<uint8_t> new_data) {
  if (IsLocal())
    return;

  // Only the name and origin dynamically change.
  if (name == kPropertyFrameOrigin) {
    web_frame_->toWebRemoteFrame()->setReplicatedOrigin(
        FrameOriginFromClientProperty(new_data));
  } else if (name == kPropertyFrameName) {
    web_frame_->toWebRemoteFrame()->setReplicatedName(
        FrameNameFromClientProperty(new_data));
  }
}

bool HTMLFrame::IsLocal() const {
  return web_frame_->isWebLocalFrame();
}

HTMLFrame* HTMLFrame::GetLocalRoot() {
  HTMLFrame* frame = this;
  while (frame && !frame->delegate_)
    frame = frame->parent_;
  return frame;
}

mojo::ApplicationImpl* HTMLFrame::GetLocalRootApp() {
  return GetLocalRoot()->delegate_->GetApp();
}

void HTMLFrame::SetView(mojo::View* view) {
  // TODO(sky): figure out way to cleanup view. In particular there may already
  // be a view. This happens if we go from local->remote->local.
  if (view_)
    view_->RemoveObserver(this);
  view_ = view;
  view_->AddObserver(this);
}

void HTMLFrame::CreateRootWebWidget() {
  DCHECK(!web_widget_);
  blink::WebViewClient* web_view_client =
      (view_ && view_->id() == id_) ? this : nullptr;
  web_widget_ = blink::WebView::create(web_view_client);

  InitializeWebWidget();

  ConfigureSettings(web_view()->settings());
}

void HTMLFrame::CreateLocalRootWebWidget(blink::WebLocalFrame* local_frame) {
  DCHECK(!web_widget_);
  DCHECK(IsLocal());
  web_widget_ = blink::WebFrameWidget::create(this, local_frame);

  InitializeWebWidget();
}

void HTMLFrame::InitializeWebWidget() {
  // Creating the widget calls initializeLayerTreeView() to create the
  // |web_layer_tree_view_impl_|. As we haven't yet assigned the |web_widget_|
  // we have to set it here.
  if (web_layer_tree_view_impl_) {
    web_layer_tree_view_impl_->set_widget(web_widget_);
    web_layer_tree_view_impl_->set_view(view_);
    UpdateWebViewSizeFromViewSize();
  }
}

void HTMLFrame::UpdateFocus() {
  if (!web_widget_ || !view_)
    return;
  const bool is_focused = view_ && view_->HasFocus();
  web_widget_->setFocus(is_focused);
  if (web_widget_->isWebView())
    static_cast<blink::WebView*>(web_widget_)->setIsActive(is_focused);
}

void HTMLFrame::UpdateWebViewSizeFromViewSize() {
  if (!web_widget_ || !view_)
    return;

  const gfx::Size size_in_pixels(view_->bounds().width, view_->bounds().height);
  const gfx::Size size_in_dips = gfx::ConvertSizeToDIP(
      view_->viewport_metrics().device_pixel_ratio, size_in_pixels);
  web_widget_->resize(
      blink::WebSize(size_in_dips.width(), size_in_dips.height()));
  web_layer_tree_view_impl_->setViewportSize(size_in_pixels);
}

void HTMLFrame::SwapToRemote(const blink::WebURLRequest& request) {
  CHECK(IsLocal());
  mojo::URLRequestPtr url_request = mojo::URLRequest::From(request);
  view_->EmbedAllowingReembed(url_request.Pass());

  // TODO(sky): I tried swapping the frame types here, but that resulted in
  // the view never getting sized. Figure out why.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&HTMLFrame::FinishSwapToRemote, weak_factory_.GetWeakPtr()));
}

void HTMLFrame::FinishSwapToRemote() {
  if (web_frame_->isWebRemoteFrame())
    return;  // We already did the swap.

  blink::WebRemoteFrame* remote_frame =
      blink::WebRemoteFrame::create(scope_, this);
  remote_frame->initializeFromFrame(web_frame_->toWebLocalFrame());
  // swap() ends up calling us back and we then close the frame, which deletes
  // it.
  web_frame_->swap(remote_frame);
  web_layer_.reset(new WebLayerImpl(this));
  remote_frame->setRemoteWebLayer(web_layer_.get());
  web_frame_ = remote_frame;
}

void HTMLFrame::SwapToLocal(
    mojo::View* view,
    const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties) {
  CHECK(!IsLocal());
  // It doesn't make sense for the root to swap to local.
  CHECK(parent_);
  SetView(view);
  blink::WebSandboxFlags sandbox_flags = blink::WebSandboxFlags::None;
  FrameSandboxFlagsFromClientProperty(
      GetValueFromClientProperties(kPropertyFrameSandboxFlags, properties),
      &sandbox_flags);
  blink::WebLocalFrame* local_web_frame =
      blink::WebLocalFrame::create(blink::WebTreeScopeType::Document, this);
  local_web_frame->initializeToReplaceRemoteFrame(
      web_frame_->toWebRemoteFrame(),
      FrameNameFromClientProperty(
          GetValueFromClientProperties(kPropertyFrameName, properties)),
      sandbox_flags);
  // The swap() ends up calling to frameDetached() and deleting the old.
  web_frame_->swap(local_web_frame);
  web_frame_ = local_web_frame;
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

void HTMLFrame::OnViewBoundsChanged(View* view,
                                    const Rect& old_bounds,
                                    const Rect& new_bounds) {
  DCHECK_EQ(view, view_);
  UpdateWebViewSizeFromViewSize();
}

void HTMLFrame::OnViewDestroyed(View* view) {
  DCHECK_EQ(view, view_);
  view_->RemoveObserver(this);
  view_ = nullptr;
  Close();
}

void HTMLFrame::OnViewInputEvent(View* view, const mojo::EventPtr& event) {
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

void HTMLFrame::OnViewFocusChanged(mojo::View* gained_focus,
                                   mojo::View* lost_focus) {
  UpdateFocus();
}

void HTMLFrame::OnConnect(mandoline::FrameTreeServerPtr server,
                          mojo::Array<mandoline::FrameDataPtr> frame_data) {
  // OnConnect() is only sent once, and has been received (by
  // DocumentResourceWaiter) by the time we get here.
  NOTREACHED();
}

void HTMLFrame::OnFrameAdded(mandoline::FrameDataPtr frame_data) {
  frame_tree_manager_->ProcessOnFrameAdded(this, frame_data.Pass());
}

void HTMLFrame::OnFrameRemoved(uint32_t frame_id) {
  frame_tree_manager_->ProcessOnFrameRemoved(this, frame_id);
}

void HTMLFrame::OnFrameClientPropertyChanged(uint32_t frame_id,
                                             const mojo::String& name,
                                             mojo::Array<uint8_t> new_value) {
  frame_tree_manager_->ProcessOnFrameClientPropertyChanged(this, frame_id, name,
                                                           new_value.Pass());
}

blink::WebStorageNamespace* HTMLFrame::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

void HTMLFrame::didCancelCompositionOnSelectionChange() {
  // TODO(penghuang): Update text input state.
}

void HTMLFrame::didChangeContents() {
  // TODO(penghuang): Update text input state.
}

void HTMLFrame::initializeLayerTreeView() {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:surfaces_service");
  mojo::SurfacePtr surface;
  GetLocalRootApp()->ConnectToService(request.Pass(), &surface);

  mojo::URLRequestPtr request2(mojo::URLRequest::New());
  request2->url = mojo::String::From("mojo:view_manager");
  mojo::GpuPtr gpu_service;
  GetLocalRootApp()->ConnectToService(request2.Pass(), &gpu_service);
  web_layer_tree_view_impl_.reset(new WebLayerTreeViewImpl(
      global_state()->compositor_thread(),
      global_state()->gpu_memory_buffer_manager(),
      global_state()->raster_thread_helper()->task_graph_runner(),
      surface.Pass(), gpu_service.Pass()));
}

blink::WebLayerTreeView* HTMLFrame::layerTreeView() {
  return web_layer_tree_view_impl_.get();
}

void HTMLFrame::resetInputMethod() {
  // When this method gets called, WebWidgetClient implementation should
  // reset the input method by cancelling any ongoing composition.
  // TODO(penghuang): Reset IME.
}

void HTMLFrame::didHandleGestureEvent(const blink::WebGestureEvent& event,
                                      bool eventCancelled) {
  // Called when a gesture event is handled.
  if (eventCancelled)
    return;

  if (event.type == blink::WebInputEvent::GestureTap) {
    const bool show_ime = true;
    UpdateTextInputState(show_ime);
  } else if (event.type == blink::WebInputEvent::GestureLongPress) {
    // Only show IME if the textfield contains text.
    const bool show_ime =
        !web_view()->textInputInfo().value.isEmpty();
    UpdateTextInputState(show_ime);
  }
}

void HTMLFrame::didUpdateTextOfFocusedElementByNonUserInput() {
  // Called when value of focused textfield gets dirty, e.g. value is
  // modified by script, not by user input.
  const bool show_ime = false;
  UpdateTextInputState(show_ime);
}

void HTMLFrame::showImeIfNeeded() {
  // Request the browser to show the IME for current input type.
  const bool show_ime = true;
  UpdateTextInputState(show_ime);
}

blink::WebMediaPlayer* HTMLFrame::createMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    blink::WebContentDecryptionModule* initial_cdm) {
  return global_state()->media_factory()->CreateMediaPlayer(
      frame, url, client, encrypted_client, initial_cdm,
      GetLocalRootApp()->shell());
}

blink::WebFrame* HTMLFrame::createChildFrame(
    blink::WebLocalFrame* parent,
    blink::WebTreeScopeType scope,
    const blink::WebString& frame_name,
    blink::WebSandboxFlags sandbox_flags) {
  DCHECK(IsLocal());  // Can't create children of remote frames.
  DCHECK_EQ(parent, web_frame_);
  DCHECK(view_);  // If we're local we have to have a view.
  // Create the view that will house the frame now. We embed once we know the
  // url (see decidePolicyForNavigation()).
  mojo::View* child_view = view_->view_manager()->CreateView();
  mojo::Map<mojo::String, mojo::Array<uint8_t>> client_properties;
  client_properties.mark_non_null();
  AddToClientPropertiesIfValid(kPropertyFrameName,
                               FrameNameToClientProperty(frame_name).Pass(),
                               &client_properties);
  AddToClientPropertiesIfValid(kPropertyFrameTreeScope,
                               FrameTreeScopeToClientProperty(scope).Pass(),
                               &client_properties);
  AddToClientPropertiesIfValid(
      kPropertyFrameSandboxFlags,
      FrameSandboxFlagsToClientProperty(sandbox_flags).Pass(),
      &client_properties);

  child_view->SetVisible(true);
  view_->AddChild(child_view);

  GetLocalRoot()->server_->OnCreatedFrame(id_, child_view->id(),
                                          client_properties.Pass());

  HTMLFrame::CreateParams params(frame_tree_manager_, this, child_view->id());
  HTMLFrame* child_frame = new HTMLFrame(params);
  child_frame->scope_ = scope;

  child_frame->SetView(child_view);

  blink::WebLocalFrame* child_web_frame =
      blink::WebLocalFrame::create(scope, child_frame);
  child_frame->web_frame_ = child_web_frame;
  parent->appendChild(child_web_frame);
  return child_web_frame;
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
  if (parent_ && parent_->IsLocal() && GetLocalRoot() != this) {
    // TODO(sky): this may be too early. I might want to wait to see if an embed
    // actually happens, and swap then.
    SwapToRemote(info.urlRequest);
    return blink::WebNavigationPolicyIgnore;
  }

  if (info.frame == web_frame() && this == frame_tree_manager_->root_ &&
      delegate_ && delegate_->ShouldNavigateLocallyInMainFrame()) {
    return info.defaultPolicy;
  }

  if (CanNavigateLocally(info.frame, info.urlRequest))
    return info.defaultPolicy;

  mojo::URLRequestPtr url_request = mojo::URLRequest::From(info.urlRequest);
  GetLocalRoot()->server_->RequestNavigate(
      id(), WebNavigationPolicyToNavigationTarget(info.defaultPolicy),
      url_request.Pass());

  return blink::WebNavigationPolicyIgnore;
}

void HTMLFrame::didAddMessageToConsole(const blink::WebConsoleMessage& message,
                                       const blink::WebString& source_name,
                                       unsigned source_line,
                                       const blink::WebString& stack_trace) {
  VLOG(1) << "[" << source_name.utf8() << "(" << source_line << ")] "
          << message.text.utf8();
}

void HTMLFrame::didFinishLoad(blink::WebLocalFrame* frame) {
  if (GetLocalRoot() == this)
    delegate_->OnFrameDidFinishLoad();
}

void HTMLFrame::didNavigateWithinPage(blink::WebLocalFrame* frame,
                                      const blink::WebHistoryItem& history_item,
                                      blink::WebHistoryCommitType commit_type) {
  GetLocalRoot()->server_->DidNavigateLocally(id_,
                                              history_item.urlString().utf8());
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
  GetLocalRoot()->server_->LoadingStarted(id_);
}

void HTMLFrame::didStopLoading() {
  GetLocalRoot()->server_->LoadingStopped(id_);
}

void HTMLFrame::didChangeLoadProgress(double load_progress) {
  GetLocalRoot()->server_->ProgressChanged(id_, load_progress);
}

void HTMLFrame::didChangeName(blink::WebLocalFrame* frame,
                              const blink::WebString& name) {
  GetLocalRoot()->server_->SetClientProperty(id_, kPropertyFrameName,
                                             FrameNameToClientProperty(name));
}

void HTMLFrame::didCommitProvisionalLoad(
    blink::WebLocalFrame* frame,
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type) {
  GetLocalRoot()->server_->SetClientProperty(
      id_, kPropertyFrameOrigin, FrameOriginToClientProperty(frame));
}

void HTMLFrame::frameDetached(blink::WebRemoteFrameClient::DetachType type) {
  if (type == blink::WebRemoteFrameClient::DetachType::Swap) {
    web_frame_->close();
    return;
  }

  DCHECK(type == blink::WebRemoteFrameClient::DetachType::Remove);
  FrameDetachedImpl(web_frame_);
}

void HTMLFrame::UpdateTextInputState(bool show_ime) {
  blink::WebTextInputInfo new_info = web_view()->textInputInfo();
  // Only show IME if the focused element is editable.
  show_ime = show_ime && new_info.type != blink::WebTextInputTypeNone;
  if (show_ime || text_input_info_ != new_info) {
    text_input_info_ = new_info;
    mojo::TextInputStatePtr state = mojo::TextInputState::New();
    state->type = mojo::ConvertTo<mojo::TextInputType>(new_info.type);
    state->flags = new_info.flags;
    state->text = mojo::String::From(new_info.value.utf8());
    state->selection_start = new_info.selectionStart;
    state->selection_end = new_info.selectionEnd;
    state->composition_start = new_info.compositionStart;
    state->composition_end = new_info.compositionEnd;
    if (show_ime)
      view_->SetImeVisibility(true, state.Pass());
    else
      view_->SetTextInputState(state.Pass());
  }
}

void HTMLFrame::postMessageEvent(blink::WebLocalFrame* source_frame,
                                 blink::WebRemoteFrame* target_frame,
                                 blink::WebSecurityOrigin target_origin,
                                 blink::WebDOMMessageEvent event) {
  NOTIMPLEMENTED();
}

void HTMLFrame::initializeChildFrame(const blink::WebRect& frame_rect,
                                     float scale_factor) {
  // TODO(sky): frame_rect is in dips. Need to convert.
  mojo::Rect rect;
  rect.x = frame_rect.x;
  rect.y = frame_rect.y;
  rect.width = frame_rect.width;
  rect.height = frame_rect.height;
  view_->SetBounds(rect);
}

void HTMLFrame::navigate(const blink::WebURLRequest& request,
                         bool should_replace_current_entry) {
  NOTIMPLEMENTED();
}

void HTMLFrame::reload(bool ignore_cache, bool is_client_redirect) {
  NOTIMPLEMENTED();
}

void HTMLFrame::forwardInputEvent(const blink::WebInputEvent* event) {
  NOTIMPLEMENTED();
}

}  // namespace mojo
