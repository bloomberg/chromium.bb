// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_ui.h"

#include <string>

#include "base/strings/string_util.h"
#include "chrome/browser/media/router/create_session_request.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_mojo_impl.h"
#include "chrome/browser/media/router/media_router_mojo_impl_factory.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/presentation_service_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/webui/media_router/media_router_localized_strings_provider.h"
#include "chrome/browser/ui/webui/media_router/media_router_resources_provider.h"
#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace media_router {

namespace {

std::string GetHostFromURL(const GURL& gurl) {
  if (gurl.is_empty())
    return std::string();
  std::string host = gurl.host();
  if (base::StartsWithASCII(host, "www.", false))
    host = host.substr(4);
  return host;
}

}  // namespace

// This class calls to refresh the UI when the highest priority issue is
// updated.
class MediaRouterUI::UIIssuesObserver : public IssuesObserver {
 public:
  UIIssuesObserver(MediaRouter* router, MediaRouterUI* ui)
      : IssuesObserver(router), ui_(ui) {
    DCHECK(ui);
  }

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
      requesting_route_for_default_source_(false),
      initiator_(nullptr),
      router_(nullptr),
      weak_factory_(this) {
  // Create a WebUIDataSource containing the chrome://media-router page's
  // content.
  scoped_ptr<content::WebUIDataSource> html_source(
      content::WebUIDataSource::Create(chrome::kChromeUIMediaRouterHost));

  content::WebContents* wc = web_ui->GetWebContents();
  DCHECK(wc);

  router_ = MediaRouterMojoImplFactory::GetApiForBrowserContext(
      wc->GetBrowserContext());
  DCHECK(router_);

  AddLocalizedStrings(html_source.get());
  AddMediaRouterUIResources(html_source.get());
  // Ownership of |html_source| is transferred to the BrowserContext.
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                html_source.release());

  // Ownership of |handler_| is transferred to |web_ui|.
  web_ui->AddMessageHandler(handler_);
}

MediaRouterUI::~MediaRouterUI() {
  if (query_result_manager_.get())
    query_result_manager_->RemoveObserver(this);
  if (presentation_service_delegate_.get())
    presentation_service_delegate_->RemoveDefaultMediaSourceObserver(this);
}

void MediaRouterUI::InitWithDefaultMediaSource(
    PresentationServiceDelegateImpl* delegate) {
  DCHECK(delegate);
  DCHECK(!presentation_service_delegate_);
  DCHECK(!query_result_manager_.get());

  presentation_service_delegate_ = delegate->GetWeakPtr();
  presentation_service_delegate_->AddDefaultMediaSourceObserver(this);
  InitCommon(presentation_service_delegate_->web_contents(),
             presentation_service_delegate_->default_source(),
             presentation_service_delegate_->default_frame_url());
}

void MediaRouterUI::InitWithPresentationSessionRequest(
    const content::WebContents* initiator,
    scoped_ptr<CreateSessionRequest> request) {
  DCHECK(request.get());
  DCHECK(!presentation_session_request_.get());
  DCHECK(!query_result_manager_.get());

  presentation_session_request_ = request.Pass();
  InitCommon(initiator, presentation_session_request_->GetMediaSource(),
             presentation_session_request_->frame_url());
}

void MediaRouterUI::InitCommon(const content::WebContents* initiator,
                               const MediaSource& default_source,
                               const GURL& default_frame_url) {
  DCHECK(initiator);

  // Register for Issue and MediaRoute updates.
  issues_observer_.reset(new UIIssuesObserver(router_, this));
  routes_observer_.reset(new UIMediaRoutesObserver(router_, this));

  query_result_manager_.reset(new QueryResultManager(router_));
  query_result_manager_->AddObserver(this);

  // These modes are always available.
  // Use the same MediaSource for all mirroring modes for now.
  // TODO(imcheng): Figure out MediaSources for the different modes.
  initiator_ = initiator;
  MediaSource mirroring_source(
      MediaSourceForTab(SessionTabHelper::IdForTab(initiator)));
  query_result_manager_->StartSinksQuery(
      MediaCastMode::DESKTOP_OR_WINDOW_MIRROR, MediaSourceForDesktop());
  query_result_manager_->StartSinksQuery(
      MediaCastMode::SOUND_OPTIMIZED_TAB_MIRROR, mirroring_source);
  query_result_manager_->StartSinksQuery(MediaCastMode::TAB_MIRROR,
                                         mirroring_source);

  OnDefaultMediaSourceChanged(default_source, default_frame_url);
}

void MediaRouterUI::OnDefaultMediaSourceChanged(const MediaSource& source,
                                                const GURL& frame_url) {
  if (source.Empty()) {
    query_result_manager_->StopSinksQuery(MediaCastMode::DEFAULT);
  } else {
    query_result_manager_->StartSinksQuery(MediaCastMode::DEFAULT, source);
  }
  UpdateSourceHostAndCastModes(frame_url);
}

void MediaRouterUI::HandleRouteResponseForPresentation(
    const MediaRoute* route,
    const std::string& error) {
  if (!route) {
    presentation_session_request_->MaybeInvokeErrorCallback(
        content::PresentationError(content::PRESENTATION_ERROR_UNKNOWN, error));
  } else {
    // TODO(imcheng): Presentation ID should come from the response
    // as the IDs might not be the same.
    presentation_session_request_->MaybeInvokeSuccessCallback();
  }
}

void MediaRouterUI::UpdateSourceHostAndCastModes(const GURL& frame_url) {
  DCHECK(query_result_manager_);
  frame_url_ = frame_url;
  query_result_manager_->GetSupportedCastModes(&cast_modes_);
  if (ui_initialized_)
    handler_->UpdateCastModes(cast_modes_, GetHostFromURL(frame_url_));
}

void MediaRouterUI::Close() {
  if (presentation_session_request_.get()) {
    presentation_session_request_->MaybeInvokeErrorCallback(
        content::PresentationError(
            content::PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED,
            "Dialog closed."));
  }

  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (delegate) {
    delegate->GetWebDialogDelegate()->OnDialogClosed(std::string());
    delegate->OnDialogCloseFromWebUI();
  }
}

void MediaRouterUI::UIInitialized() {
  ui_initialized_ = true;
}

bool MediaRouterUI::CreateRoute(const MediaSink::Id& sink_id) {
  return DoCreateRoute(sink_id, GetPreferredCastMode(cast_modes_));
}

bool MediaRouterUI::CreateRouteWithCastModeOverride(
    const MediaSink::Id& sink_id,
    MediaCastMode cast_mode_override) {
  // NOTE: It's actually not an override if
  // |cast_mode_override| == |GetPreferredCastMode(cast_modes_)|.
  return DoCreateRoute(sink_id, cast_mode_override);
}

void MediaRouterUI::CloseRoute(const MediaRoute::Id& route_id) {
  router_->CloseRoute(route_id);
}

void MediaRouterUI::ClearIssue(const std::string& issue_id) {
  router_->ClearIssue(issue_id);
}

std::string MediaRouterUI::GetInitialHeaderText() const {
  if (cast_modes_.empty())
    return std::string();

  return MediaCastModeToTitle(GetPreferredCastMode(cast_modes_),
                              GetHostFromURL(frame_url_));
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

  if (requesting_route_for_default_source_) {
    if (presentation_session_request_.get()) {
      HandleRouteResponseForPresentation(route.get(), error);
    } else {
      // Dialog initiated via browser action. Let
      // PresentationServiceDelegateImpl perform the match against the default
      // presentation URL.
      if (route && presentation_service_delegate_.get())
        presentation_service_delegate_->OnRouteCreated(*route);
    }
  }

  has_pending_route_request_ = false;
  requesting_route_for_default_source_ = false;
}

bool MediaRouterUI::DoCreateRoute(const MediaSink::Id& sink_id,
                                  MediaCastMode cast_mode) {
  DCHECK(query_result_manager_.get());
  DCHECK(initiator_);

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
  requesting_route_for_default_source_ = cast_mode == MediaCastMode::DEFAULT;
  GURL origin;
  // TODO(imcheng): What is the origin if not creating route in DEFAULT mode?
  if (requesting_route_for_default_source_) {
    origin = frame_url_.GetOrigin();
  } else {
    // Requesting route for mirroring. Use a placeholder URL as origin.
    origin = GURL(chrome::kChromeUIMediaRouterURL);
  }
  DCHECK(origin.is_valid());

  DVLOG(1) << "DoCreateRoute: origin: " << origin;

  router_->CreateRoute(source.id(), sink_id, origin,
                       SessionTabHelper::IdForTab(initiator_),
                       base::Bind(&MediaRouterUI::OnRouteResponseReceived,
                                  weak_factory_.GetWeakPtr()));
  return true;
}

std::string MediaRouterUI::GetFrameURLHost() const {
  return GetHostFromURL(frame_url_);
}

}  // namespace media_router
