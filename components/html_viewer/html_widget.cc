// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_widget.h"

#include "base/command_line.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/ime_controller.h"
#include "components/html_viewer/stats_collection_controller.h"
#include "components/html_viewer/web_layer_tree_view_impl.h"
#include "components/html_viewer/web_storage_namespace_impl.h"
#include "components/mus/public/cpp/view.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/services/tracing/public/interfaces/tracing.mojom.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/geometry/dip_util.h"

namespace html_viewer {
namespace {

const char kDisableWebGLSwitch[] = "disable-webgl";

scoped_ptr<WebLayerTreeViewImpl> CreateWebLayerTreeView(
    GlobalState* global_state) {
  return make_scoped_ptr(new WebLayerTreeViewImpl(
      global_state->compositor_thread(),
      global_state->gpu_memory_buffer_manager(),
      global_state->raster_thread_helper()->task_graph_runner()));
}

void InitializeWebLayerTreeView(WebLayerTreeViewImpl* web_layer_tree_view,
                                mojo::ApplicationImpl* app,
                                mus::View* view,
                                blink::WebWidget* widget) {
  DCHECK(view);
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:mus");
  mojo::GpuPtr gpu_service;
  app->ConnectToService(request.Pass(), &gpu_service);
  web_layer_tree_view->Initialize(gpu_service.Pass(), view, widget);
}

void UpdateWebViewSizeFromViewSize(mus::View* view,
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

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  settings->setExperimentalWebGLEnabled(
      !command_line->HasSwitch(kDisableWebGLSwitch));
}

}  // namespace

// HTMLWidgetRootRemote -------------------------------------------------------

HTMLWidgetRootRemote::HTMLWidgetRootRemote()
    : web_view_(blink::WebView::create(this)) {
  ConfigureSettings(web_view_->settings());
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

void HTMLWidgetRootRemote::OnViewBoundsChanged(mus::View* view) {}

// HTMLWidgetRootLocal --------------------------------------------------------

HTMLWidgetRootLocal::CreateParams::CreateParams(mojo::ApplicationImpl* app,
                                                GlobalState* global_state,
                                                mus::View* view)
    : app(app), global_state(global_state), view(view) {}

HTMLWidgetRootLocal::CreateParams::~CreateParams() {}

HTMLWidgetRootLocal::HTMLWidgetRootLocal(CreateParams* create_params)
    : app_(create_params->app),
      global_state_(create_params->global_state),
      view_(create_params->view),
      web_view_(nullptr) {
  web_view_ = blink::WebView::create(this);
  ime_controller_.reset(new ImeController(view_, web_view_));
  // Creating the widget calls initializeLayerTreeView() to create the
  // |web_layer_tree_view_impl_|. As we haven't yet assigned the |web_view_|
  // we have to set it here.
  if (web_layer_tree_view_impl_) {
    InitializeWebLayerTreeView(web_layer_tree_view_impl_.get(), app_, view_,
                               web_view_);
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

void HTMLWidgetRootLocal::initializeLayerTreeView() {
  web_layer_tree_view_impl_ = CreateWebLayerTreeView(global_state_);
}

blink::WebLayerTreeView* HTMLWidgetRootLocal::layerTreeView() {
  return web_layer_tree_view_impl_.get();
}

void HTMLWidgetRootLocal::didFirstVisuallyNonEmptyLayout() {
  static bool called = false;
  if (!called) {
    const int64 time = base::Time::Now().ToInternalValue();
    tracing::StartupPerformanceDataCollectorPtr collector =
        StatsCollectionController::ConnectToDataCollector(app_);
    if (collector)
      collector->SetFirstVisuallyNonEmptyLayoutTime(time);
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

void HTMLWidgetRootLocal::OnViewBoundsChanged(mus::View* view) {
  UpdateWebViewSizeFromViewSize(view, web_view_,
                                web_layer_tree_view_impl_.get());
}

// HTMLWidgetLocalRoot --------------------------------------------------------

HTMLWidgetLocalRoot::HTMLWidgetLocalRoot(mojo::ApplicationImpl* app,
                                         GlobalState* global_state,
                                         mus::View* view,
                                         blink::WebLocalFrame* web_local_frame)
    : app_(app), global_state_(global_state), web_frame_widget_(nullptr) {
  web_frame_widget_ = blink::WebFrameWidget::create(this, web_local_frame);
  ime_controller_.reset(new ImeController(view, web_frame_widget_));
  // Creating the widget calls initializeLayerTreeView() to create the
  // |web_layer_tree_view_impl_|. As we haven't yet assigned the
  // |web_frame_widget_|
  // we have to set it here.
  if (web_layer_tree_view_impl_) {
    InitializeWebLayerTreeView(web_layer_tree_view_impl_.get(), app_, view,
                               web_frame_widget_);
    UpdateWebViewSizeFromViewSize(view, web_frame_widget_,
                                  web_layer_tree_view_impl_.get());
  }
}

HTMLWidgetLocalRoot::~HTMLWidgetLocalRoot() {}

blink::WebWidget* HTMLWidgetLocalRoot::GetWidget() {
  return web_frame_widget_;
}

void HTMLWidgetLocalRoot::OnViewBoundsChanged(mus::View* view) {
  UpdateWebViewSizeFromViewSize(view, web_frame_widget_,
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
