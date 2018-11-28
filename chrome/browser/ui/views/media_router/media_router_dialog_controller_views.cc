// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_webui_impl.h"

namespace media_router {

// static
MediaRouterDialogControllerImplBase*
MediaRouterDialogControllerImplBase::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  if (ShouldUseViewsDialog()) {
    return MediaRouterDialogControllerViews::GetOrCreateForWebContents(
        web_contents);
  } else {
    return MediaRouterDialogControllerWebUIImpl::GetOrCreateForWebContents(
        web_contents);
  }
}

MediaRouterDialogControllerViews::~MediaRouterDialogControllerViews() {
  Reset();
  if (CastDialogView::GetCurrentDialogWidget())
    CastDialogView::GetCurrentDialogWidget()->RemoveObserver(this);
}

// static
MediaRouterDialogControllerViews*
MediaRouterDialogControllerViews::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // This call does nothing if the controller already exists.
  MediaRouterDialogControllerViews::CreateForWebContents(web_contents);
  return MediaRouterDialogControllerViews::FromWebContents(web_contents);
}

void MediaRouterDialogControllerViews::CreateMediaRouterDialog() {
  base::Time dialog_creation_time = base::Time::Now();
  MediaRouterDialogControllerImplBase::CreateMediaRouterDialog();

  Browser* browser = chrome::FindBrowserWithWebContents(initiator());
  if (!browser)
    return;

  ui_ = std::make_unique<MediaRouterViewsUI>();
  InitializeMediaRouterUI(ui_.get());
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (browser_view->toolbar()->cast_button()) {
    CastDialogView::ShowDialogWithToolbarAction(ui_.get(), browser,
                                                dialog_creation_time);
  } else {
    CastDialogView::ShowDialogTopCentered(ui_.get(), browser,
                                          dialog_creation_time);
  }
  CastDialogView::GetCurrentDialogWidget()->AddObserver(this);
  if (dialog_creation_callback_)
    dialog_creation_callback_.Run();
}

void MediaRouterDialogControllerViews::CloseMediaRouterDialog() {
  CastDialogView::HideDialog();
}

bool MediaRouterDialogControllerViews::IsShowingMediaRouterDialog() const {
  return CastDialogView::IsShowing();
}

void MediaRouterDialogControllerViews::Reset() {
  // If |ui_| is null, Reset() has already been called.
  if (ui_) {
    MediaRouterDialogControllerImplBase::Reset();
    ui_.reset();
  }
}

void MediaRouterDialogControllerViews::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(CastDialogView::GetCurrentDialogWidget(), widget);
  Reset();
}

void MediaRouterDialogControllerViews::OnWidgetDestroying(
    views::Widget* widget) {
  widget->RemoveObserver(this);
}

void MediaRouterDialogControllerViews::SetDialogCreationCallbackForTesting(
    base::RepeatingClosure callback) {
  dialog_creation_callback_ = std::move(callback);
}

MediaRouterDialogControllerViews::MediaRouterDialogControllerViews(
    content::WebContents* web_contents)
    : MediaRouterDialogControllerImplBase(web_contents) {}

}  // namespace media_router
