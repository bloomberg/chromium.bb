// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screensaver/screensaver_view.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/bind.h"
#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using content::BrowserThread;

namespace {

ash::internal::ScreensaverView* g_instance = NULL;

// Do not restart the screensaver again if it has
// terminated kMaxTerminations times already.
const int kMaxTerminations = 3;

}  // namespace

namespace ash {

void ShowScreensaver(const GURL& url) {
  internal::ScreensaverView::ShowScreensaver(url);
}

void CloseScreensaver() {
  internal::ScreensaverView::CloseScreensaver();
}

namespace internal {

// static
void ScreensaverView::ShowScreensaver(const GURL& url) {
  if (!g_instance) {
    g_instance = new ScreensaverView(url);
    g_instance->Show();
  }
}

// static
void ScreensaverView::CloseScreensaver() {
  if (g_instance) {
    g_instance->Close();
    g_instance = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ScreensaverView, views::WidgetDelegateView implementation.
views::View* ScreensaverView::GetContentsView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// ScreensaverView, content::WebContentsObserver implementation.
void ScreensaverView::RenderViewGone(
    base::TerminationStatus status) {
  LOG(ERROR) << "Screensaver terminated with status " << status;
  termination_count_++;

  if (termination_count_ < kMaxTerminations) {
    LOG(ERROR) << termination_count_
               << " terminations is under the threshold of "
               << kMaxTerminations
               << "; reloading Screensaver.";
    LoadScreensaver();
  } else {
    LOG(ERROR) << "Exceeded termination threshold, closing Screensaver.";
    ScreensaverView::CloseScreensaver();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ScreensaverView private methods.
ScreensaverView::ScreensaverView(const GURL& url)
    : url_(url),
      termination_count_(0),
      screensaver_webview_(NULL),
      container_window_(NULL) {
}

ScreensaverView::~ScreensaverView() {
}

void ScreensaverView::Show() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Add the WebView to our view.
  AddChildWebContents();
  // Show the window.
  ShowWindow();
}

void ScreensaverView::Close() {
  DCHECK(GetWidget());
  GetWidget()->Close();
}

void ScreensaverView::AddChildWebContents() {
  content::BrowserContext* context =
      Shell::GetInstance()->delegate()->GetCurrentBrowserContext();
  screensaver_webview_ = new views::WebView(context);
  SetLayoutManager(new views::FillLayout);
  AddChildView(screensaver_webview_);

  LoadScreensaver();
  content::WebContentsObserver::Observe(
      screensaver_webview_->GetWebContents());
}

void ScreensaverView::LoadScreensaver() {
  screensaver_webview_->GetWebContents()->GetController().LoadURL(
        url_,
        content::Referrer(),
        content::PAGE_TRANSITION_AUTO_TOPLEVEL,
        std::string());
}

void ScreensaverView::ShowWindow() {
  aura::RootWindow* root_window = ash::Shell::GetPrimaryRootWindow();
  gfx::Rect screen_rect =
      gfx::Screen::GetDisplayNearestWindow(root_window).bounds();

  // We want to be the fullscreen topmost child of the root window.
  // There should be nothing ever really that should show up on top of us.
  container_window_ = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = this;
  params.parent = root_window;
  container_window_->Init(params);

  container_window_->StackAtTop();
  container_window_->SetBounds(screen_rect);
  container_window_->Show();
}

// static
ScreensaverView* ScreensaverView::GetInstance() {
  return g_instance;
}

}  // namespace internal
}  // namespace ash
