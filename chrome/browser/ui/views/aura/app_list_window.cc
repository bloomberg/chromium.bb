// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/app_list_window.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "views/widget/widget.h"
#include "ui/aura/desktop.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/screen.h"

namespace {

// Gets preferred bounds of app list window in show/hide state.
gfx::Rect GetPreferredBounds(bool show) {
  // The y-axis offset used at the beginning of showing animation.
  static const int kMoveUpAnimationOffset = 50;

  gfx::Point cursor = gfx::Screen::GetCursorScreenPoint();
  gfx::Rect work_area = gfx::Screen::GetMonitorWorkAreaNearestPoint(cursor);
  gfx::Rect widget_bounds(work_area);
  widget_bounds.Inset(150, 100);
  if (!show)
    widget_bounds.Offset(0, kMoveUpAnimationOffset);

  return widget_bounds;
}

ui::Layer* GetWidgetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

}  // namespace

// static
AppListWindow* AppListWindow::instance_ = NULL;

// static
void AppListWindow::SetVisible(bool visible) {
  if (!instance_) {
    instance_ = new AppListWindow;
    instance_->Init();
  }

  instance_->DoSetVisible(visible);
}

// static
bool AppListWindow::IsVisible() {
  return instance_ && instance_->is_visible_;
}

AppListWindow::AppListWindow()
    : widget_(NULL),
      contents_(NULL),
      is_visible_(false),
      content_rendered_(false) {
}

AppListWindow::~AppListWindow() {
}

void AppListWindow::DeleteDelegate() {
  delete this;
}

views::View* AppListWindow::GetContentsView() {
  return contents_;
}

void AppListWindow::WindowClosing() {
  aura::Desktop::GetInstance()->RemoveObserver(this);
  widget_ = NULL;
}

views::Widget* AppListWindow::GetWidget() {
  return widget_;
}

const views::Widget* AppListWindow::GetWidget() const {
  return widget_;
}

void AppListWindow::OnActiveWindowChanged(aura::Window* active) {
  if (widget_ && !widget_->IsActive() && is_visible_)
    DoSetVisible(false);
}

void AppListWindow::OnLayerAnimationEnded(
    const ui::LayerAnimationSequence* sequence) {
  if (!is_visible_ )
    widget_->Close();
}

void AppListWindow::OnLayerAnimationAborted(
    const ui::LayerAnimationSequence* sequence) {
}

void AppListWindow::OnLayerAnimationScheduled(
    const ui::LayerAnimationSequence* sequence) {
}

void AppListWindow::OnRenderHostCreated(RenderViewHost* host) {
}

void AppListWindow::OnTabMainFrameLoaded() {
}

void AppListWindow::OnTabMainFrameFirstRender() {
  content_rendered_ = true;

  // Do deferred show animation if necessary.
  if (is_visible_ && GetWidgetLayer(widget_)->opacity() == 0) {
    is_visible_ = false;
    DoSetVisible(true);
  }
}

void AppListWindow::Init() {
  DCHECK(!widget_ && !contents_);

  contents_ = new DOMView();
  contents_->Init(ProfileManager::GetDefaultProfile(), NULL);

  TabContents* tab = contents_->dom_contents()->tab_contents();
  tab_watcher_.reset(new TabFirstRenderWatcher(tab, this));
  content_rendered_ = false;

  contents_->LoadURL(GURL(chrome::kChromeUIAppListURL));

  // Use a background with transparency to trigger transparent webkit.
  SkBitmap background;
  background.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
  background.allocPixels();
  background.eraseARGB(0x00, 0x00, 0x00, 0x00);

  RenderViewHost* host = tab->render_view_host();
  host->view()->SetBackground(background);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.bounds = GetPreferredBounds(false);
  widget_params.delegate = this;
  widget_params.keep_on_top = true;
  widget_params.transparent = true;

  widget_ = new views::Widget;
  widget_->Init(widget_params);
  widget_->SetContentsView(contents_);
  widget_->SetOpacity(0);

  GetWidgetLayer(widget_)->GetAnimator()->AddObserver(this);
  aura::Desktop::GetInstance()->AddObserver(this);
}

void AppListWindow::DoSetVisible(bool visible) {
  if (visible == is_visible_)
    return;

  is_visible_ = visible;

  // Skip show animation if contents is not rendered.
  // TODO(xiyuan): Should we show a loading UI if it takes too long?
  if (visible && !content_rendered_)
    return;

  ui::Layer* layer = GetWidgetLayer(widget_);
  ui::LayerAnimator::ScopedSettings settings(layer->GetAnimator());
  layer->SetBounds(GetPreferredBounds(visible));
  layer->SetOpacity(visible ? 1.0 : 0.0);

  if (visible) {
    widget_->Show();
    widget_->Activate();
  } else {
    instance_ = NULL;  // Closing and don't reuse this instance_.
  }
}
