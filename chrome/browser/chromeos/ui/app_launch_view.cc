// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/app_launch_view.h"

#include <string>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using content::BrowserThread;

namespace chromeos {

internal::AppLaunchView* g_instance = NULL;

void ShowAppLaunchSplashScreen() {
  // TODO(zelidrag): Come up with a better UI for this purpose.
  internal::AppLaunchView::ShowAppLaunchSplashScreen();
}

void CloseAppLaunchSplashScreen() {
  internal::AppLaunchView::CloseAppLaunchSplashScreen();
}

namespace internal {

// static
void AppLaunchView::ShowAppLaunchSplashScreen() {
  if (!g_instance) {
    g_instance = new AppLaunchView();
    g_instance->Show();
  }
}

// static
void AppLaunchView::CloseAppLaunchSplashScreen() {
  if (g_instance) {
    g_instance->Close();
    g_instance = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppLaunchView, views::WidgetDelegateView implementation.
views::View* AppLaunchView::GetContentsView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// AppLaunchView, content::WebContentsObserver implementation.
void AppLaunchView::RenderViewGone(
    base::TerminationStatus status) {
  LOG(ERROR) << "Splash screen terminated with status " << status;
  AppLaunchView::CloseAppLaunchSplashScreen();
}

////////////////////////////////////////////////////////////////////////////////
// AppLaunchView private methods.
AppLaunchView::AppLaunchView()
    : app_launch_webview_(NULL),
      container_window_(NULL) {
}

AppLaunchView::~AppLaunchView() {
}

void AppLaunchView::Show() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Add the WebView to our view.
  AddChildWebContents();
  // Initialize container window.
  InitializeWindow();
  // Show the window.
  ShowWindow();
}

void AppLaunchView::Close() {
  DCHECK(GetWidget());
  GetWidget()->Close();
}

void AppLaunchView::AddChildWebContents() {
  content::BrowserContext* context =
      ash::Shell::GetInstance()->delegate()->GetCurrentBrowserContext();
  app_launch_webview_ = new views::WebView(context);
  SetLayoutManager(new views::FillLayout);
  AddChildView(app_launch_webview_);

  LoadSplashScreen();
  content::WebContentsObserver::Observe(
      app_launch_webview_->GetWebContents());
}

void AppLaunchView::LoadSplashScreen() {
  app_launch_webview_->GetWebContents()->GetController().LoadURL(
      GURL(chrome::kChromeUIAppLaunchURL),
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

void AppLaunchView::InitializeWindow() {
  DCHECK(!container_window_);
  aura::RootWindow* root_window = ash::Shell::GetPrimaryRootWindow();

  // We want to be the fullscreen topmost child of the root window.
  // There should be nothing ever really that should show up on top of us.
  container_window_ = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = this;
  params.parent = ash::Shell::GetContainer(
      root_window,
      ash::internal::kShellWindowId_LockScreenContainer);
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  container_window_->Init(params);
}

void AppLaunchView::ShowWindow() {
  DCHECK(container_window_);
  container_window_->Show();
}

}  // namespace internal

}  // namespace chromeos
