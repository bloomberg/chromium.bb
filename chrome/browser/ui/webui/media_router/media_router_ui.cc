// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_ui.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/guid.h"
#include "base/i18n/string_compare.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/create_presentation_connection_request.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"
#include "chrome/browser/media/router/presentation_service_delegate_impl.h"
#include "chrome/browser/media/router/route_request_result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/webui/media_router/media_router_localized_strings_provider.h"
#include "chrome/browser/ui/webui/media_router/media_router_resources_provider.h"
#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace media_router {

namespace {

// The amount of time to wait for a response when creating a new route.
const int kCreateRouteTimeoutSeconds = 20;
const int kCreateRouteTimeoutSecondsForTab = 60;
const int kCreateRouteTimeoutSecondsForDesktop = 120;

std::string GetHostFromURL(const GURL& gurl) {
  if (gurl.is_empty()) return std::string();
  std::string host = gurl.host();
  if (base::StartsWith(host, "www.", base::CompareCase::INSENSITIVE_ASCII))
    host = host.substr(4);
  return host;
}

std::string TruncateHost(const std::string& host) {
  const std::string truncated =
      net::registry_controlled_domains::GetDomainAndRegistry(
          host, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  // The truncation will be empty in some scenarios (e.g. host is
  // simply an IP address). Fail gracefully.
  return truncated.empty() ? host : truncated;
}

base::TimeDelta GetRouteRequestTimeout(MediaCastMode cast_mode) {
  switch (cast_mode) {
    case DEFAULT:
      return base::TimeDelta::FromSeconds(kCreateRouteTimeoutSeconds);
    case TAB_MIRROR:
      return base::TimeDelta::FromSeconds(kCreateRouteTimeoutSecondsForTab);
    case DESKTOP_MIRROR:
      return base::TimeDelta::FromSeconds(kCreateRouteTimeoutSecondsForDesktop);
    default:
      NOTREACHED();
      return base::TimeDelta();
  }
}

}  // namespace

// static
std::string MediaRouterUI::GetExtensionName(
    const GURL& gurl, extensions::ExtensionRegistry* registry) {
  if (gurl.is_empty() || !registry) return std::string();

  const extensions::Extension* extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(gurl);

  return extension ? extension->name() : std::string();
}

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

MediaRouterUI::UIMediaRoutesObserver::UIMediaRoutesObserver(
    MediaRouter* router, const MediaSource::Id& source_id,
    const RoutesUpdatedCallback& callback)
    : MediaRoutesObserver(router, source_id), callback_(callback) {
  DCHECK(!callback_.is_null());
}

MediaRouterUI::UIMediaRoutesObserver::~UIMediaRoutesObserver() {}

void MediaRouterUI::UIMediaRoutesObserver::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes,
    const std::vector<MediaRoute::Id>& joinable_route_ids) {
  std::vector<MediaRoute> routes_for_display;
  std::vector<MediaRoute::Id> joinable_route_ids_for_display;
  for (const MediaRoute& route : routes) {
    if (route.for_display()) {
#ifndef NDEBUG
      for (const MediaRoute& existing_route : routes_for_display) {
        if (existing_route.media_sink_id() == route.media_sink_id()) {
          DVLOG(2) << "Received another route for display with the same sink"
                   << " id as an existing route. " << route.media_route_id()
                   << " has the same sink id as "
                   << existing_route.media_sink_id() << ".";
        }
      }
#endif
      if (ContainsValue(joinable_route_ids, route.media_route_id())) {
        joinable_route_ids_for_display.push_back(route.media_route_id());
      }

      routes_for_display.push_back(route);
    }
  }

  callback_.Run(routes_for_display, joinable_route_ids_for_display);
}

MediaRouterUI::MediaRouterUI(content::WebUI* web_ui)
    : ConstrainedWebDialogUI(web_ui),
      handler_(new MediaRouterWebUIMessageHandler(this)),
      ui_initialized_(false),
      current_route_request_id_(-1),
      route_request_counter_(0),
      initiator_(nullptr),
      router_(nullptr),
      weak_factory_(this) {
  // Create a WebUIDataSource containing the chrome://media-router page's
  // content.
  std::unique_ptr<content::WebUIDataSource> html_source(
      content::WebUIDataSource::Create(chrome::kChromeUIMediaRouterHost));

  content::WebContents* wc = web_ui->GetWebContents();
  DCHECK(wc);

  router_ =
      MediaRouterFactory::GetApiForBrowserContext(wc->GetBrowserContext());

  // Allows UI to load extensionview.
  // TODO(haibinlu): limit object-src to current extension once crbug/514866
  // is fixed.
  html_source->OverrideContentSecurityPolicyObjectSrc("object-src *;");

  AddLocalizedStrings(html_source.get());
  AddMediaRouterUIResources(html_source.get());
  // Ownership of |html_source| is transferred to the BrowserContext.
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                html_source.release());

  // Ownership of |handler_| is transferred to |web_ui|.
  web_ui->AddMessageHandler(handler_);
}

MediaRouterUI::~MediaRouterUI() {
  if (issues_observer_) issues_observer_->UnregisterObserver();

  if (query_result_manager_.get()) query_result_manager_->RemoveObserver(this);
  if (presentation_service_delegate_.get())
    presentation_service_delegate_->RemoveDefaultPresentationRequestObserver(
        this);
  // If |create_session_request_| still exists, then it means presentation route
  // request was never attempted.
  if (create_session_request_) {
    create_session_request_->InvokeErrorCallback(content::PresentationError(
        content::PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED,
        "Dialog closed."));
  }
}

void MediaRouterUI::InitWithDefaultMediaSource(
    const base::WeakPtr<PresentationServiceDelegateImpl>& delegate) {
  DCHECK(delegate);
  DCHECK(!presentation_service_delegate_);
  DCHECK(!query_result_manager_.get());

  presentation_service_delegate_ = delegate;
  presentation_service_delegate_->AddDefaultPresentationRequestObserver(this);
  InitCommon(presentation_service_delegate_->web_contents());
  if (presentation_service_delegate_->HasDefaultPresentationRequest()) {
    OnDefaultPresentationChanged(
        presentation_service_delegate_->GetDefaultPresentationRequest());
  } else {
    // Register for MediaRoute updates without a media source.
    routes_observer_.reset(new UIMediaRoutesObserver(router_, MediaSource::Id(),
          base::Bind(&MediaRouterUI::OnRoutesUpdated, base::Unretained(this))));
  }
}

void MediaRouterUI::InitWithPresentationSessionRequest(
    content::WebContents* initiator,
    const base::WeakPtr<PresentationServiceDelegateImpl>& delegate,
    std::unique_ptr<CreatePresentationConnectionRequest>
        create_session_request) {
  DCHECK(initiator);
  DCHECK(create_session_request);
  DCHECK(!create_session_request_);
  DCHECK(!query_result_manager_);

  create_session_request_ = std::move(create_session_request);
  presentation_service_delegate_ = delegate;
  InitCommon(initiator);
  OnDefaultPresentationChanged(create_session_request_->presentation_request());
}

void MediaRouterUI::InitCommon(content::WebContents* initiator) {
  DCHECK(initiator);
  DCHECK(router_);

  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("media_router", "UI", initiator,
                                      "MediaRouterUI::InitCommon", this);

  router_->OnUserGesture();

  // Create |collator_| before |query_result_manager_| so that |collator_| is
  // already set up when we get a callback from |query_result_manager_|.
  UErrorCode error = U_ZERO_ERROR;
  const std::string& locale = g_browser_process->GetApplicationLocale();
  collator_.reset(
      icu::Collator::createInstance(icu::Locale(locale.c_str()), error));
  if (U_FAILURE(error)) {
    DLOG(ERROR) << "Failed to create collator for locale " << locale;
    collator_.reset();
  }

  query_result_manager_.reset(new QueryResultManager(router_));
  query_result_manager_->AddObserver(this);

  // Use a placeholder URL as origin for mirroring.
  GURL origin(chrome::kChromeUIMediaRouterURL);

  // Desktop mirror mode is always available.
  query_result_manager_->StartSinksQuery(MediaCastMode::DESKTOP_MIRROR,
                                         MediaSourceForDesktop(), origin);
  initiator_ = initiator;
  SessionID::id_type tab_id = SessionTabHelper::IdForTab(initiator);
  if (tab_id != -1) {
    MediaSource mirroring_source(MediaSourceForTab(tab_id));
    query_result_manager_->StartSinksQuery(MediaCastMode::TAB_MIRROR,
                                           mirroring_source, origin);
  }
  UpdateCastModes();
}

void MediaRouterUI::InitForTest(MediaRouter* router,
                                content::WebContents* initiator,
                                MediaRouterWebUIMessageHandler* handler) {
  router_ = router;
  handler_ = handler;
  InitCommon(initiator);
}

void MediaRouterUI::OnDefaultPresentationChanged(
    const PresentationRequest& presentation_request) {
  MediaSource source = presentation_request.GetMediaSource();
  presentation_request_.reset(new PresentationRequest(presentation_request));
  query_result_manager_->StartSinksQuery(
      MediaCastMode::DEFAULT, source,
      presentation_request_->frame_url().GetOrigin());
  // Register for MediaRoute updates.
  routes_observer_.reset(new UIMediaRoutesObserver(
      router_, source.id(),
      base::Bind(&MediaRouterUI::OnRoutesUpdated, base::Unretained(this))));

  UpdateCastModes();
}

void MediaRouterUI::OnDefaultPresentationRemoved() {
  presentation_request_.reset();
  query_result_manager_->StopSinksQuery(MediaCastMode::DEFAULT);
  // Register for MediaRoute updates without a media source.
  routes_observer_.reset(new UIMediaRoutesObserver(
        router_, MediaSource::Id(),
        base::Bind(&MediaRouterUI::OnRoutesUpdated, base::Unretained(this))));
  UpdateCastModes();
}

void MediaRouterUI::UpdateCastModes() {
  // Gets updated cast modes from |query_result_manager_| and forwards it to UI.
  query_result_manager_->GetSupportedCastModes(&cast_modes_);
  if (ui_initialized_) {
    handler_->UpdateCastModes(cast_modes_, GetPresentationRequestSourceName());
  }
}

void MediaRouterUI::Close() {
  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (delegate) {
    delegate->GetWebDialogDelegate()->OnDialogClosed(std::string());
    delegate->OnDialogCloseFromWebUI();
  }
}

void MediaRouterUI::UIInitialized() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("media_router", "UI", initiator_);
  ui_initialized_ = true;

  // Register for Issue updates.
  if (!issues_observer_)
    issues_observer_.reset(new UIIssuesObserver(router_, this));
  issues_observer_->RegisterObserver();
}

bool MediaRouterUI::CreateRoute(const MediaSink::Id& sink_id,
                                MediaCastMode cast_mode) {
  return CreateOrConnectRoute(sink_id, cast_mode, MediaRoute::Id());
}

bool MediaRouterUI::CreateOrConnectRoute(const MediaSink::Id& sink_id,
                                              MediaCastMode cast_mode,
                                              const MediaRoute::Id& route_id) {
  DCHECK(query_result_manager_.get());
  DCHECK(initiator_);

  // Note that there is a rarely-encountered bug, where the MediaCastMode to
  // MediaSource mapping could have been updated, between when the user clicked
  // on the UI to start a create route request, and when this function is
  // called. However, since the user does not have visibility into the
  // MediaSource, and that it occurs very rarely in practice, we leave it as-is
  // for now.
  MediaSource source = query_result_manager_->GetSourceForCastMode(cast_mode);
  if (source.Empty()) {
    LOG(ERROR) << "No corresponding MediaSource for cast mode "
               << static_cast<int>(cast_mode);
    return false;
  }

  bool for_default_source = cast_mode == MediaCastMode::DEFAULT;
  if (for_default_source && !presentation_request_) {
    DLOG(ERROR) << "Requested to create a route for presentation, but "
                << "presentation request is missing.";
    return false;
  }

  current_route_request_id_ = ++route_request_counter_;
  GURL origin = for_default_source
                    ? presentation_request_->frame_url().GetOrigin()
                    : GURL(chrome::kChromeUIMediaRouterURL);
  DCHECK(origin.is_valid());

  DVLOG(1) << "DoCreateRoute: origin: " << origin;

  // There are 3 cases. In all cases the MediaRouterUI will need to be notified.
  // (1) Non-presentation route request (e.g., mirroring). No additional
  // notification necessary.
  // (2) Presentation route request for a Presentation API startSession call.
  // The startSession (CreatePresentationConnectionRequest) will need to be
  // answered with the
  // route response.
  // (3) Browser-initiated presentation route request. If successful,
  // PresentationServiceDelegateImpl will have to be notified. Note that we
  // treat subsequent route requests from a Presentation API-initiated dialogs
  // as browser-initiated.
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &MediaRouterUI::OnRouteResponseReceived, weak_factory_.GetWeakPtr(),
      current_route_request_id_, sink_id, cast_mode,
      base::UTF8ToUTF16(GetTruncatedPresentationRequestSourceName())));
  if (for_default_source) {
    if (create_session_request_) {
      // |create_session_request_| will be nullptr after this call, as the
      // object will be transferred to the callback.
      route_response_callbacks.push_back(
          base::Bind(&CreatePresentationConnectionRequest::HandleRouteResponse,
                     base::Passed(&create_session_request_)));
    } else if (presentation_service_delegate_) {
      route_response_callbacks.push_back(
          base::Bind(&PresentationServiceDelegateImpl::OnRouteResponse,
                     presentation_service_delegate_, *presentation_request_));
    }
  }

  base::TimeDelta timeout = GetRouteRequestTimeout(cast_mode);
  bool off_the_record = Profile::FromWebUI(web_ui())->IsOffTheRecord();
  if (route_id.empty()) {
    router_->CreateRoute(source.id(), sink_id, origin, initiator_,
                         route_response_callbacks, timeout,
                         off_the_record);
  } else {
    router_->ConnectRouteByRouteId(source.id(), route_id, origin, initiator_,
                                   route_response_callbacks, timeout,
                                   off_the_record);
  }
  return true;
}

bool MediaRouterUI::ConnectRoute(const MediaSink::Id& sink_id,
                                 const MediaRoute::Id& route_id) {
  return CreateOrConnectRoute(sink_id, MediaCastMode::DEFAULT, route_id);
}

void MediaRouterUI::CloseRoute(const MediaRoute::Id& route_id) {
  router_->TerminateRoute(route_id);
}

void MediaRouterUI::AddIssue(const Issue& issue) { router_->AddIssue(issue); }

void MediaRouterUI::ClearIssue(const std::string& issue_id) {
  router_->ClearIssue(issue_id);
}

void MediaRouterUI::OnResultsUpdated(
    const std::vector<MediaSinkWithCastModes>& sinks) {
  sinks_ = sinks;

  const icu::Collator* collator_ptr = collator_.get();
  std::sort(
      sinks_.begin(), sinks_.end(),
      [collator_ptr](const MediaSinkWithCastModes& sink1,
                     const MediaSinkWithCastModes& sink2) {
        if (collator_ptr) {
          base::string16 sink1_name = base::UTF8ToUTF16(sink1.sink.name());
          base::string16 sink2_name = base::UTF8ToUTF16(sink2.sink.name());
          UCollationResult result = base::i18n::CompareString16WithCollator(
              *collator_ptr, sink1_name, sink2_name);
          if (result != UCOL_EQUAL)
            return result == UCOL_LESS;
        } else {
          // Fall back to simple string comparison if collator is not
          // available.
          int val = sink1.sink.name().compare(sink2.sink.name());
          if (val)
            return val < 0;
        }

        return sink1.sink.id() < sink2.sink.id();
      });

  if (ui_initialized_) handler_->UpdateSinks(sinks_);
}

void MediaRouterUI::SetIssue(const Issue* issue) {
  if (ui_initialized_) handler_->UpdateIssue(issue);
}

void MediaRouterUI::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes,
    const std::vector<MediaRoute::Id>& joinable_route_ids) {
  routes_ = routes;
  joinable_route_ids_ = joinable_route_ids;
  if (ui_initialized_) handler_->UpdateRoutes(routes_, joinable_route_ids_);
}

void MediaRouterUI::OnRouteResponseReceived(
    int route_request_id,
    const MediaSink::Id& sink_id,
    MediaCastMode cast_mode,
    const base::string16& presentation_request_source_name,
    const RouteRequestResult& result) {
  DVLOG(1) << "OnRouteResponseReceived";
  // If we receive a new route that we aren't expecting, do nothing.
  if (route_request_id != current_route_request_id_) return;

  const MediaRoute* route = result.route();
  if (!route) {
    // The provider will handle sending an issue for a failed route request.
    DVLOG(1) << "MediaRouteResponse returned error: " << result.error();
  }

  handler_->OnCreateRouteResponseReceived(sink_id, route);
  current_route_request_id_ = -1;

  if (result.result_code() == RouteRequestResult::TIMED_OUT)
    SendIssueForRouteTimeout(cast_mode, presentation_request_source_name);
}

void MediaRouterUI::SendIssueForRouteTimeout(
    MediaCastMode cast_mode,
    const base::string16& presentation_request_source_name) {
  std::string issue_title;
  switch (cast_mode) {
    case DEFAULT:
      DLOG_IF(ERROR, presentation_request_source_name.empty())
          << "Empty presentation request source name.";
      issue_title =
          l10n_util::GetStringFUTF8(IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT,
                                    presentation_request_source_name);
      break;
    case TAB_MIRROR:
      issue_title = l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_TAB);
      break;
    case DESKTOP_MIRROR:
      issue_title = l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_DESKTOP);
      break;
    default:
      NOTREACHED();
  }

  Issue issue(issue_title, std::string(),
              IssueAction(IssueAction::TYPE_DISMISS),
              std::vector<IssueAction>(), std::string(), Issue::NOTIFICATION,
              false, std::string());
  AddIssue(issue);
}

GURL MediaRouterUI::GetFrameURL() const {
  return presentation_request_ ? presentation_request_->frame_url() : GURL();
}

std::string MediaRouterUI::GetPresentationRequestSourceName() const {
  GURL gurl = GetFrameURL();
  return gurl.SchemeIs(extensions::kExtensionScheme)
             ? GetExtensionName(gurl, extensions::ExtensionRegistry::Get(
                                          Profile::FromWebUI(web_ui())))
             : GetHostFromURL(gurl);
}

std::string MediaRouterUI::GetTruncatedPresentationRequestSourceName() const {
  GURL gurl = GetFrameURL();
  return gurl.SchemeIs(extensions::kExtensionScheme)
             ? GetExtensionName(gurl, extensions::ExtensionRegistry::Get(
                                          Profile::FromWebUI(web_ui())))
             : TruncateHost(GetHostFromURL(gurl));
}

const std::string& MediaRouterUI::GetRouteProviderExtensionId() const {
  // TODO(crbug.com/597778): remove reference to MediaRouterMojoImpl
  return static_cast<MediaRouterMojoImpl*>(router_)
      ->media_route_provider_extension_id();
}

void MediaRouterUI::SetUIInitializationTimer(const base::Time& start_time) {
  DCHECK(!start_time.is_null());
  start_time_ = start_time;
}

void MediaRouterUI::OnUIInitiallyLoaded() {
  if (!start_time_.is_null()) {
    MediaRouterMetrics::RecordMediaRouterDialogPaint(
        base::Time::Now() - start_time_);
  }
}

void MediaRouterUI::OnUIInitialDataReceived() {
  if (!start_time_.is_null()) {
    MediaRouterMetrics::RecordMediaRouterDialogLoaded(
        base::Time::Now() - start_time_);
    start_time_ = base::Time();
  }
}

void MediaRouterUI::UpdateMaxDialogHeight(int height) {
  handler_->UpdateMaxDialogHeight(height);
}

}  // namespace media_router
