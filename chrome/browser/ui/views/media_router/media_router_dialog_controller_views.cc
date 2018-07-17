// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_webui_impl.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    media_router::MediaRouterDialogControllerViews);

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
  BrowserActionsContainer* browser_actions =
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar()
          ->browser_actions();
  // |browser_actions| may be null in toolbar-less browser windows.
  // TODO(takumif): Show the dialog at the top-middle of the window if the
  // toolbar is missing.
  if (!browser_actions)
    return;
  views::View* action_view = browser_actions->GetViewForId(
      ComponentToolbarActionsFactory::kMediaRouterActionId);

  ui_ = std::make_unique<MediaRouterViewsUI>();
  InitializeMediaRouterUI(ui_.get());
  CastDialogView::ShowDialog(action_view, ui_.get(), browser,
                             dialog_creation_time);
  CastDialogView::GetCurrentDialogWidget()->AddObserver(this);
}

void MediaRouterDialogControllerViews::CloseMediaRouterDialog() {
  CastDialogView::HideDialog();
}

bool MediaRouterDialogControllerViews::IsShowingMediaRouterDialog() const {
  return CastDialogView::IsShowing();
}

void MediaRouterDialogControllerViews::Reset() {
  MediaRouterDialogControllerImplBase::Reset();
  ui_.reset();
}

void MediaRouterDialogControllerViews::OnWidgetClosing(views::Widget* widget) {
  Reset();
}

MediaRouterDialogControllerViews::MediaRouterDialogControllerViews(
    content::WebContents* web_contents)
    : MediaRouterDialogControllerImplBase(web_contents) {}

}  // namespace media_router
