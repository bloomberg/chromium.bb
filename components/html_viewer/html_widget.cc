// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_widget.h"

#include <stdint.h>
#include <utility>

#include "base/command_line.h"
#include "components/html_viewer/blink_settings.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/ime_controller.h"
#include "components/html_viewer/stats_collection_controller.h"
#include "components/html_viewer/web_layer_tree_view_impl.h"
#include "components/html_viewer/web_storage_namespace_impl.h"
#include "components/mus/public/cpp/window.h"
#include "mojo/services/tracing/public/interfaces/tracing.mojom.h"
#include "mojo/shell/public/cpp/shell.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/geometry/dip_util.h"

namespace html_viewer {
namespace {

scoped_ptr<WebLayerTreeViewImpl> CreateWebLayerTreeView(
    GlobalState* global_state) {
  return make_scoped_ptr(new WebLayerTreeViewImpl(
      global_state->compositor_thread(),
      global_state->gpu_memory_buffer_manager(),
      global_state->raster_thread_helper()->task_graph_runner()));
}

void InitializeWebLayerTreeView(WebLayerTreeViewImpl* web_layer_tree_view,
                                mojo::Shell* shell,
                                mus::Window* window,
                                blink::WebWidget* widget) {
  DCHECK(window);
  mus::mojom::GpuPtr gpu_service;
  shell->ConnectToService("mojo:mus", &gpu_service);
  web_layer_tree_view->Initialize(std::move(gpu_service), window, widget);
}

void UpdateWebViewSizeFromViewSize(mus::Window* window,
                                   blink::WebWidget* web_widget,
                                   WebLayerTreeViewImpl* web_layer_tree_view) {
  const gfx::Size size_in_pixels(window->bounds().size());
  const gfx::Size size_in_dips = gfx::ConvertSizeToDIP(
      window->viewport_metrics().device_pixel_ratio, size_in_pixels);
  web_widget->resize(
      blink::WebSize(size_in_dips.width(), size_in_dips.height()));
  web_layer_tree_view->setViewportSize(size_in_pixels);
}

}  // namespace

// HTMLWidgetRootRemote -------------------------------------------------------

HTMLWidgetRootRemote::HTMLWidgetRootRemote(GlobalState* global_state)
    : web_view_(blink::WebView::create(this)) {
  global_state->blink_settings()->ApplySettingsToWebView(web_view_);
}

HTMLWidgetRootRemote::~HTMLWidgetRootRemote() {}

blink::WebStorageNamespace*
HTMLWidgetRootRemote::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

bool HTMLWidgetRootRemote::allowsBrokenNullLayerTreeView() const {
  return true;
}

blink::WebWidget* HTMLWidgetRootRemote::GetWidget() {
  return web_view_;
}

void HTMLWidgetRootRemote::OnWindowBoundsChanged(mus::Window* window) {}

// HTMLWidgetRootLocal --------------------------------------------------------

HTMLWidgetRootLocal::CreateParams::CreateParams(mojo::Shell* shell,
                                                GlobalState* global_state,
                                                mus::Window* window)
    : shell(shell), global_state(global_state), window(window) {}

HTMLWidgetRootLocal::CreateParams::~CreateParams() {}

HTMLWidgetRootLocal::HTMLWidgetRootLocal(CreateParams* create_params)
    : shell_(create_params->shell),
      global_state_(create_params->global_state),
      window_(create_params->window),
      web_view_(nullptr) {
  web_view_ = blink::WebView::create(this);
  ime_controller_.reset(new ImeController(window_, web_view_));
  // Creating the widget calls initializeLayerTreeView() to create the
  // |web_layer_tree_view_impl_|. As we haven't yet assigned the |web_view_|
  // we have to set it here.
  if (web_layer_tree_view_impl_) {
    InitializeWebLayerTreeView(web_layer_tree_view_impl_.get(), shell_, window_,
                               web_view_);
    UpdateWebViewSizeFromViewSize(window_, web_view_,
                                  web_layer_tree_view_impl_.get());
  }
  global_state_->blink_settings()->ApplySettingsToWebView(web_view_);
}

HTMLWidgetRootLocal::~HTMLWidgetRootLocal() {}

blink::WebStorageNamespace*
HTMLWidgetRootLocal::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

void HTMLWidgetRootLocal::initializeLayerTreeView() {
  web_layer_tree_view_impl_ = CreateWebLayerTreeView(global_state_);
}

blink::WebLayerTreeView* HTMLWidgetRootLocal::layerTreeView() {
  return web_layer_tree_view_impl_.get();
}

void HTMLWidgetRootLocal::didMeaningfulLayout(
    blink::WebMeaningfulLayout layout_type) {
  static bool called = false;
  if (!called && layout_type == blink::WebMeaningfulLayout::VisuallyNonEmpty) {
    const int64_t ticks = base::TimeTicks::Now().ToInternalValue();
    tracing::StartupPerformanceDataCollectorPtr collector =
        StatsCollectionController::ConnectToDataCollector(shell_);
    if (collector)
      collector->SetFirstVisuallyNonEmptyLayoutTicks(ticks);
    called = true;
  }
}

void HTMLWidgetRootLocal::resetInputMethod() {
  ime_controller_->ResetInputMethod();
}

void HTMLWidgetRootLocal::didHandleGestureEvent(
    const blink::WebGestureEvent& event,
    bool event_cancelled) {
  ime_controller_->DidHandleGestureEvent(event, event_cancelled);
}

void HTMLWidgetRootLocal::didUpdateTextOfFocusedElementByNonUserInput() {
  ime_controller_->DidUpdateTextOfFocusedElementByNonUserInput();
}

void HTMLWidgetRootLocal::showImeIfNeeded() {
  ime_controller_->ShowImeIfNeeded();
}

blink::WebWidget* HTMLWidgetRootLocal::GetWidget() {
  return web_view_;
}

void HTMLWidgetRootLocal::OnWindowBoundsChanged(mus::Window* window) {
  UpdateWebViewSizeFromViewSize(window, web_view_,
                                web_layer_tree_view_impl_.get());
}

// HTMLWidgetLocalRoot --------------------------------------------------------

HTMLWidgetLocalRoot::HTMLWidgetLocalRoot(mojo::Shell* shell,
                                         GlobalState* global_state,
                                         mus::Window* window,
                                         blink::WebLocalFrame* web_local_frame)
    : shell_(shell), global_state_(global_state), web_frame_widget_(nullptr) {
  web_frame_widget_ = blink::WebFrameWidget::create(this, web_local_frame);
  ime_controller_.reset(new ImeController(window, web_frame_widget_));
  // Creating the widget calls initializeLayerTreeView() to create the
  // |web_layer_tree_view_impl_|. As we haven't yet assigned the
  // |web_frame_widget_|
  // we have to set it here.
  if (web_layer_tree_view_impl_) {
    InitializeWebLayerTreeView(web_layer_tree_view_impl_.get(), shell_, window,
                               web_frame_widget_);
    UpdateWebViewSizeFromViewSize(window, web_frame_widget_,
                                  web_layer_tree_view_impl_.get());
  }
}

HTMLWidgetLocalRoot::~HTMLWidgetLocalRoot() {}

blink::WebWidget* HTMLWidgetLocalRoot::GetWidget() {
  return web_frame_widget_;
}

void HTMLWidgetLocalRoot::OnWindowBoundsChanged(mus::Window* window) {
  UpdateWebViewSizeFromViewSize(window, web_frame_widget_,
                                web_layer_tree_view_impl_.get());
}

void HTMLWidgetLocalRoot::initializeLayerTreeView() {
  web_layer_tree_view_impl_ = CreateWebLayerTreeView(global_state_);
}

blink::WebLayerTreeView* HTMLWidgetLocalRoot::layerTreeView() {
  return web_layer_tree_view_impl_.get();
}

void HTMLWidgetLocalRoot::resetInputMethod() {
  ime_controller_->ResetInputMethod();
}

void HTMLWidgetLocalRoot::didHandleGestureEvent(
    const blink::WebGestureEvent& event,
    bool event_cancelled) {
  ime_controller_->DidHandleGestureEvent(event, event_cancelled);
}

void HTMLWidgetLocalRoot::didUpdateTextOfFocusedElementByNonUserInput() {
  ime_controller_->DidUpdateTextOfFocusedElementByNonUserInput();
}

void HTMLWidgetLocalRoot::showImeIfNeeded() {
  ime_controller_->ShowImeIfNeeded();
}

}  // namespace html_viewer
