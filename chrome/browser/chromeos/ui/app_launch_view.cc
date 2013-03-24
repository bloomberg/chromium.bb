// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/app_launch_view.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/app_launch_ui.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using content::BrowserThread;

namespace chromeos {

internal::AppLaunchView* g_instance = NULL;

void ShowAppLaunchSplashScreen(const std::string& app_id) {
  // Disables the default white rendering from RenderWidgetHostViewAura.
  aura::Env::GetInstance()->set_render_white_bg(false);

  // TODO(zelidrag): Come up with a better UI for this purpose.
  internal::AppLaunchView::ShowAppLaunchSplashScreen(app_id);
}

void UpdateAppLaunchSplashScreenState(AppLaunchState state) {
  internal::AppLaunchView::UpdateAppLaunchState(state);
}

void CloseAppLaunchSplashScreen() {
  internal::AppLaunchView::CloseAppLaunchSplashScreen();

  // Re-enables the default white rendering from RenderWidgetHostViewAura.
  aura::Env::GetInstance()->set_render_white_bg(true);
}

namespace internal {

int GetProgressMessageFromState(AppLaunchState state) {
  switch (state) {
    case APP_LAUNCH_STATE_PREPARING_NETWORK:
      return IDS_APP_START_NETWORK_WAIT_MESSAGE;
    case APP_LAUNCH_STATE_INSTALLING_APPLICATION:
      return IDS_APP_START_APP_WAIT_MESSAGE;
  }
  return IDS_APP_START_NETWORK_WAIT_MESSAGE;
}

// static
void AppLaunchView::ShowAppLaunchSplashScreen(const std::string& app_id) {
  if (!g_instance) {
    g_instance = new AppLaunchView(app_id);
    g_instance->Show();
  }
}

void AppLaunchView::UpdateAppLaunchState(AppLaunchState state) {
  if (g_instance)
    g_instance->UpdateState(state);
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
AppLaunchView::AppLaunchView(const std::string& app_id)
    : app_launch_webview_(NULL),
      container_window_(NULL),
      app_id_(app_id),
      state_(APP_LAUNCH_STATE_PREPARING_NETWORK),
      app_launch_ui_(NULL) {
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

void AppLaunchView::UpdateState(AppLaunchState state) {
  if (state == state_)
    return;

  state_ = state;
  if (!app_launch_ui_)
    return;

  app_launch_ui_->SetLaunchText(
      l10n_util::GetStringUTF8(GetProgressMessageFromState(state_)));
}

void AppLaunchView::LoadSplashScreen() {
  std::string url = chrome::kChromeUIAppLaunchURL;
  url += "?app=" + app_id_;

  app_launch_webview_->GetWebContents()->GetController().LoadURL(
      GURL(url),
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());

  app_launch_ui_ = static_cast<AppLaunchUI*>(
      app_launch_webview_->GetWebContents()->GetWebUI()->GetController());

  // Use a background with transparency to trigger transparency in Webkit.
  SkBitmap background;
  background.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
  background.allocPixels();
  background.eraseARGB(0x00, 0x00, 0x00, 0x00);
  content::RenderViewHost* host =
      app_launch_webview_->GetWebContents()->GetRenderViewHost();
  host->GetView()->SetBackground(background);
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
