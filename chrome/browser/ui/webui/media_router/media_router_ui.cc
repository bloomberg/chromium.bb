// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_ui.h"

#include <string>

#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_mojo_impl.h"
#include "chrome/browser/media/router/media_router_mojo_impl_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/media_router/media_router_localized_strings_provider.h"
#include "chrome/browser/ui/webui/media_router/media_router_resources_provider.h"
#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace media_router {

// This class calls to refresh the UI when the highest priority issue is
// updated.
class MediaRouterUI::UIIssuesObserver : public IssuesObserver {
 public:
  explicit UIIssuesObserver(MediaRouterUI* ui) : ui_(ui) { DCHECK(ui); }

  ~UIIssuesObserver() override {}

  // IssuesObserver implementation.
  void OnIssueUpdated(const Issue* issue) override { ui_->SetIssue(issue); }

 private:
  // Reference back to the owning MediaRouterUI instance.
  MediaRouterUI* ui_;

  DISALLOW_COPY_AND_ASSIGN(UIIssuesObserver);
};

class MediaRouterUI::UIMediaRoutesObserver : public MediaRoutesObserver {
 public:
  UIMediaRoutesObserver(MediaRouter* router, MediaRouterUI* ui)
      : MediaRoutesObserver(router), ui_(ui) {
    DCHECK(ui_);
  }

  void OnRoutesUpdated(const std::vector<MediaRoute>& routes) override {
    ui_->OnRoutesUpdated(routes);
  }

 private:
  // Reference back to the owning MediaRouterUI instance.
  MediaRouterUI* ui_;

  DISALLOW_COPY_AND_ASSIGN(UIMediaRoutesObserver);
};

MediaRouterUI::MediaRouterUI(content::WebUI* web_ui)
    : ConstrainedWebDialogUI(web_ui),
      handler_(new MediaRouterWebUIMessageHandler()),
      ui_initialized_(false),
      has_pending_route_request_(false),
      router_(nullptr),
      weak_factory_(this) {
  // Create a WebUIDataSource containing the chrome://media-router page's
  // content.
  scoped_ptr<content::WebUIDataSource> html_source(
      content::WebUIDataSource::Create(chrome::kChromeUIMediaRouterHost));
  AddLocalizedStrings(html_source.get());
  AddMediaRouterUIResources(html_source.get());
  // Ownership of |html_source| is transferred to the BrowserContext.
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                html_source.release());

  // Ownership of |handler_| is transferred to |web_ui|.
  web_ui->AddMessageHandler(handler_);

  content::WebContents* wc = web_ui->GetWebContents();
  DCHECK(wc);

  router_ = MediaRouterMojoImplFactory::GetApiForBrowserContext(
      wc->GetBrowserContext());
  DCHECK(router_);

  // Register for Issue and MediaRoute updates.
  issues_observer_.reset(new UIIssuesObserver(this));
  routes_observer_.reset(new UIMediaRoutesObserver(router_, this));
}

MediaRouterUI::~MediaRouterUI() {
  if (query_result_manager_.get())
    query_result_manager_->RemoveObserver(this);
}

void MediaRouterUI::Close() {
  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (delegate) {
    delegate->GetWebDialogDelegate()->OnDialogClosed(std::string());
    delegate->OnDialogCloseFromWebUI();
  }
}

void MediaRouterUI::UIInitialized() {
  ui_initialized_ = true;
}

bool MediaRouterUI::CreateRoute(const MediaSinkId& sink_id) {
  return DoCreateRoute(sink_id, GetPreferredCastMode(cast_modes_));
}

bool MediaRouterUI::CreateRouteWithCastModeOverride(
    const MediaSinkId& sink_id,
    MediaCastMode cast_mode_override) {
  // NOTE: It's actually not an override if
  // |cast_mode_override| == |GetPreferredCastMode(cast_modes_)|.
  return DoCreateRoute(sink_id, cast_mode_override);
}

void MediaRouterUI::CloseRoute(const MediaRouteId& route_id) {
  router_->CloseRoute(route_id);
}

void MediaRouterUI::ClearIssue(const std::string& issue_id) {
  router_->ClearIssue(issue_id);
}

std::string MediaRouterUI::GetInitialHeaderText() const {
  if (cast_modes_.empty())
    return std::string();

  // TODO(imcheng): Pass in source_host_ once DEFAULT mode is upstreamed.
  return MediaCastModeToTitle(GetPreferredCastMode(cast_modes_), std::string());
}

void MediaRouterUI::OnResultsUpdated(
    const std::vector<MediaSinkWithCastModes>& sinks) {
  sinks_ = sinks;
  if (ui_initialized_)
    handler_->UpdateSinks(sinks_);
}

void MediaRouterUI::SetIssue(const Issue* issue) {
  if (ui_initialized_)
    handler_->UpdateIssue(issue);
}

void MediaRouterUI::OnRoutesUpdated(const std::vector<MediaRoute>& routes) {
  routes_ = routes;
  if (ui_initialized_)
    handler_->UpdateRoutes(routes_);
}

void MediaRouterUI::OnRouteResponseReceived(scoped_ptr<MediaRoute> route,
                                            const std::string& error) {
  DVLOG(1) << "OnRouteResponseReceived";
  // TODO(imcheng): Display error in UI. (crbug.com/490372)
  if (!route)
    LOG(ERROR) << "MediaRouteResponse returned error: " << error;
  else
    handler_->AddRoute(*route);

  has_pending_route_request_ = false;
}

bool MediaRouterUI::DoCreateRoute(const MediaSinkId& sink_id,
                                  MediaCastMode cast_mode) {
  DCHECK(query_result_manager_.get());

  // Note that there is a rarely-encountered bug, where the MediaCastMode to
  // MediaSource mapping could have been updated, between when the user
  // clicked on the UI to start a create route request,
  // and when this function is called.
  // However, since the user does not have visibility into the MediaSource, and
  // that it occurs very rarely in practice, we leave it as-is for now.
  MediaSource source = query_result_manager_->GetSourceForCastMode(cast_mode);
  if (source.Empty()) {
    LOG(ERROR) << "No corresponding MediaSource for cast mode " << cast_mode;
    return false;
  }

  has_pending_route_request_ = true;
  router_->CreateRoute(source.id(), sink_id,
                       base::Bind(&MediaRouterUI::OnRouteResponseReceived,
                                  weak_factory_.GetWeakPtr()));
  return true;
}

}  // namespace media_router

