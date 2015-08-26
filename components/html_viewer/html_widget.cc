// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_widget.h"

#include "components/html_viewer/blink_input_events_type_converters.h"
#include "components/html_viewer/blink_text_input_type_converters.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/web_layer_tree_view_impl.h"
#include "components/html_viewer/web_storage_namespace_impl.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/geometry/dip_util.h"

namespace html_viewer {
namespace {

scoped_ptr<WebLayerTreeViewImpl> CreateWebLayerTreeView(
    mojo::ApplicationImpl* app,
    GlobalState* global_state) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:view_manager");
  mojo::SurfacePtr surface;
  app->ConnectToService(request.Pass(), &surface);

  mojo::URLRequestPtr request2(mojo::URLRequest::New());
  request2->url = mojo::String::From("mojo:view_manager");
  mojo::GpuPtr gpu_service;
  app->ConnectToService(request2.Pass(), &gpu_service);
  return make_scoped_ptr(new WebLayerTreeViewImpl(
      global_state->compositor_thread(),
      global_state->gpu_memory_buffer_manager(),
      global_state->raster_thread_helper()->task_graph_runner(), surface.Pass(),
      gpu_service.Pass()));
}

void UpdateWebViewSizeFromViewSize(mojo::View* view,
                                   blink::WebWidget* web_widget,
                                   WebLayerTreeViewImpl* web_layer_tree_view) {
  const gfx::Size size_in_pixels(view->bounds().width, view->bounds().height);
  const gfx::Size size_in_dips = gfx::ConvertSizeToDIP(
      view->viewport_metrics().device_pixel_ratio, size_in_pixels);
  web_widget->resize(
      blink::WebSize(size_in_dips.width(), size_in_dips.height()));
  web_layer_tree_view->setViewportSize(size_in_pixels);
}

void ConfigureSettings(blink::WebSettings* settings) {
  settings->setCookieEnabled(true);
  settings->setDefaultFixedFontSize(13);
  settings->setDefaultFontSize(16);
  settings->setLoadsImagesAutomatically(true);
  settings->setJavaScriptEnabled(true);
}

}  // namespace

// HTMLWidgetRootRemote -------------------------------------------------------

HTMLWidgetRootRemote::HTMLWidgetRootRemote()
    : web_view_(blink::WebView::create(nullptr)) {
  ConfigureSettings(web_view_->settings());
}

HTMLWidgetRootRemote::~HTMLWidgetRootRemote() {}

blink::WebWidget* HTMLWidgetRootRemote::GetWidget() {
  return web_view_;
}

void HTMLWidgetRootRemote::OnViewBoundsChanged(mojo::View* view) {}

// HTMLWidgetRootLocal --------------------------------------------------------

HTMLWidgetRootLocal::CreateParams::CreateParams(mojo::ApplicationImpl* app,
                                                GlobalState* global_state,
                                                mojo::View* view)
    : app(app), global_state(global_state), view(view) {}

HTMLWidgetRootLocal::CreateParams::~CreateParams() {}

HTMLWidgetRootLocal::HTMLWidgetRootLocal(CreateParams* create_params)
    : app_(create_params->app),
      global_state_(create_params->global_state),
      view_(create_params->view),
      web_view_(nullptr) {
  web_view_ = blink::WebView::create(this);
  // Creating the widget calls initializeLayerTreeView() to create the
  // |web_layer_tree_view_impl_|. As we haven't yet assigned the |web_view_|
  // we have to set it here.
  if (web_layer_tree_view_impl_) {
    web_layer_tree_view_impl_->set_widget(web_view_);
    web_layer_tree_view_impl_->set_view(create_params->view);
    UpdateWebViewSizeFromViewSize(view_, web_view_,
                                  web_layer_tree_view_impl_.get());
  }
  ConfigureSettings(web_view_->settings());
}

HTMLWidgetRootLocal::~HTMLWidgetRootLocal() {}

blink::WebStorageNamespace*
HTMLWidgetRootLocal::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

void HTMLWidgetRootLocal::didCancelCompositionOnSelectionChange() {
  // TODO(penghuang): Update text input state.
}

void HTMLWidgetRootLocal::didChangeContents() {
  // TODO(penghuang): Update text input state.
}

void HTMLWidgetRootLocal::initializeLayerTreeView() {
  web_layer_tree_view_impl_ = CreateWebLayerTreeView(app_, global_state_);
}

blink::WebLayerTreeView* HTMLWidgetRootLocal::layerTreeView() {
  return web_layer_tree_view_impl_.get();
}

void HTMLWidgetRootLocal::resetInputMethod() {
  // When this method gets called, WebWidgetClient implementation should
  // reset the input method by cancelling any ongoing composition.
  // TODO(penghuang): Reset IME.
}

void HTMLWidgetRootLocal::didHandleGestureEvent(
    const blink::WebGestureEvent& event,
    bool event_cancelled) {
  // Called when a gesture event is handled.
  if (event_cancelled)
    return;

  if (event.type == blink::WebInputEvent::GestureTap) {
    const bool show_ime = true;
    UpdateTextInputState(show_ime);
  } else if (event.type == blink::WebInputEvent::GestureLongPress) {
    // Only show IME if the textfield contains text.
    const bool show_ime = !web_view_->textInputInfo().value.isEmpty();
    UpdateTextInputState(show_ime);
  }
}

void HTMLWidgetRootLocal::didUpdateTextOfFocusedElementByNonUserInput() {
  // Called when value of focused textfield gets dirty, e.g. value is
  // modified by script, not by user input.
  const bool show_ime = false;
  UpdateTextInputState(show_ime);
}

void HTMLWidgetRootLocal::showImeIfNeeded() {
  // Request the browser to show the IME for current input type.
  const bool show_ime = true;
  UpdateTextInputState(show_ime);
}

void HTMLWidgetRootLocal::UpdateTextInputState(bool show_ime) {
  blink::WebTextInputInfo new_info = web_view_->textInputInfo();
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

blink::WebWidget* HTMLWidgetRootLocal::GetWidget() {
  return web_view_;
}

void HTMLWidgetRootLocal::OnViewBoundsChanged(mojo::View* view) {
  UpdateWebViewSizeFromViewSize(view, web_view_,
                                web_layer_tree_view_impl_.get());
}

// HTMLWidgetLocalRoot --------------------------------------------------------

HTMLWidgetLocalRoot::HTMLWidgetLocalRoot(mojo::ApplicationImpl* app,
                                         GlobalState* global_state,
                                         mojo::View* view,
                                         blink::WebLocalFrame* web_local_frame)
    : app_(app), global_state_(global_state), web_frame_widget_(nullptr) {
  web_frame_widget_ = blink::WebFrameWidget::create(this, web_local_frame);
  // Creating the widget calls initializeLayerTreeView() to create the
  // |web_layer_tree_view_impl_|. As we haven't yet assigned the
  // |web_frame_widget_|
  // we have to set it here.
  if (web_layer_tree_view_impl_) {
    web_layer_tree_view_impl_->set_widget(web_frame_widget_);
    web_layer_tree_view_impl_->set_view(view);
    UpdateWebViewSizeFromViewSize(view, web_frame_widget_,
                                  web_layer_tree_view_impl_.get());
  }
}

HTMLWidgetLocalRoot::~HTMLWidgetLocalRoot() {}

blink::WebWidget* HTMLWidgetLocalRoot::GetWidget() {
  return web_frame_widget_;
}

void HTMLWidgetLocalRoot::OnViewBoundsChanged(mojo::View* view) {
  UpdateWebViewSizeFromViewSize(view, web_frame_widget_,
                                web_layer_tree_view_impl_.get());
}

void HTMLWidgetLocalRoot::initializeLayerTreeView() {
  web_layer_tree_view_impl_ = CreateWebLayerTreeView(app_, global_state_);
}

blink::WebLayerTreeView* HTMLWidgetLocalRoot::layerTreeView() {
  return web_layer_tree_view_impl_.get();
}

}  // namespace html_viewer
