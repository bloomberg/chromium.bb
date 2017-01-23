// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window_linux.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace chromecast {
namespace shell {

// static
std::unique_ptr<CastContentWindow> CastContentWindow::Create(
    CastContentWindow::Delegate* delegate) {
  DCHECK(delegate);
  return base::WrapUnique(new CastContentWindowLinux());
}

CastContentWindowLinux::CastContentWindowLinux() : transparent_(false) {}

CastContentWindowLinux::~CastContentWindowLinux() {}

void CastContentWindowLinux::SetTransparent() {
  transparent_ = true;
}

std::unique_ptr<content::WebContents> CastContentWindowLinux::CreateWebContents(
    content::BrowserContext* browser_context) {
  CHECK(display::Screen::GetScreen());
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();

  content::WebContents::CreateParams create_params(browser_context, NULL);
  create_params.routing_id = MSG_ROUTING_NONE;
  create_params.initial_size = display_size;
  content::WebContents* web_contents =
      content::WebContents::Create(create_params);

#if defined(USE_AURA)
  // Resize window
  aura::Window* content_window = web_contents->GetNativeView();
  content_window->SetBounds(
      gfx::Rect(display_size.width(), display_size.height()));
#endif

  content::WebContentsObserver::Observe(web_contents);
  return base::WrapUnique(web_contents);
}

void CastContentWindowLinux::ShowWebContents(
    content::WebContents* web_contents,
    CastWindowManager* window_manager) {
  DCHECK(window_manager);
  window_manager->AddWindow(web_contents->GetNativeView());
  web_contents->GetNativeView()->Show();
}

void CastContentWindowLinux::DidFirstVisuallyNonEmptyPaint() {
  metrics::CastMetricsHelper::GetInstance()->LogTimeToFirstPaint();
}

void CastContentWindowLinux::MediaStartedPlaying(
    const MediaPlayerInfo& media_info,
    const MediaPlayerId& id) {
  metrics::CastMetricsHelper::GetInstance()->LogMediaPlay();
}

void CastContentWindowLinux::MediaStoppedPlaying(
    const MediaPlayerInfo& media_info,
    const MediaPlayerId& id) {
  metrics::CastMetricsHelper::GetInstance()->LogMediaPause();
}

void CastContentWindowLinux::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  content::RenderWidgetHostView* view =
      render_view_host->GetWidget()->GetView();
  if (view) {
    view->SetBackgroundColor(transparent_ ? SK_ColorTRANSPARENT
                                          : SK_ColorBLACK);
  }
}

}  // namespace shell
}  // namespace chromecast
