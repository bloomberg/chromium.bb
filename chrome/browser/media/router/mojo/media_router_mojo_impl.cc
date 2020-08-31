// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/browser/media/cast_mirroring_service_host.h"
#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/mojo/media_route_provider_util_win.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/browser/media/router/mojo/media_sink_service_status.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/grit/chromium_strings.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/base/l10n/l10n_util.h"

#define DVLOG_WITH_INSTANCE(level) \
  DVLOG(level) << "MR #" << instance_id_ << ": "

#define DLOG_WITH_INSTANCE(level) DLOG(level) << "MR #" << instance_id_ << ": "

namespace media_router {

namespace {

// TODO(crbug.com/831416): Delete temporary code once we can use
// presentation.mojom types here.
blink::mojom::PresentationConnectionCloseReason
PresentationConnectionCloseReasonToBlink(
    mojom::MediaRouter::PresentationConnectionCloseReason reason) {
  switch (reason) {
    case mojom::MediaRouter::PresentationConnectionCloseReason::
        CONNECTION_ERROR:
      return blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR;
    case mojom::MediaRouter::PresentationConnectionCloseReason::CLOSED:
      return blink::mojom::PresentationConnectionCloseReason::CLOSED;
    case mojom::MediaRouter::PresentationConnectionCloseReason::WENT_AWAY:
      return blink::mojom::PresentationConnectionCloseReason::WENT_AWAY;
  }
  NOTREACHED() << "Unknown PresentationConnectionCloseReason " << reason;
  return blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR;
}

// TODO(crbug.com/831416): Delete temporary code once we can use
// presentation.mojom types here.
blink::mojom::PresentationConnectionState PresentationConnectionStateToBlink(
    mojom::MediaRouter::PresentationConnectionState state) {
  switch (state) {
    case mojom::MediaRouter::PresentationConnectionState::CONNECTING:
      return blink::mojom::PresentationConnectionState::CONNECTING;
    case mojom::MediaRouter::PresentationConnectionState::CONNECTED:
      return blink::mojom::PresentationConnectionState::CONNECTED;
    case mojom::MediaRouter::PresentationConnectionState::CLOSED:
      return blink::mojom::PresentationConnectionState::CLOSED;
    case mojom::MediaRouter::PresentationConnectionState::TERMINATED:
      return blink::mojom::PresentationConnectionState::TERMINATED;
  }
  NOTREACHED() << "Unknown PresentationConnectionState " << state;
  return blink::mojom::PresentationConnectionState::CONNECTING;
}

// Get the WebContents associated with the given tab id. Returns nullptr if the
// tab id is invalid, or if the searching fails.
// TODO(xjz): Move this to SessionTabHelper to allow it being used by
// extensions::ExtensionTabUtil::GetTabById() as well.
content::WebContents* GetWebContentsFromId(
    int32_t tab_id,
    content::BrowserContext* browser_context,
    bool include_incognito) {
  if (tab_id < 0)
    return nullptr;
  Profile* profile = Profile::FromBrowserContext(browser_context);
  Profile* incognito_profile =
      include_incognito && profile->HasOffTheRecordProfile()
          ? profile->GetOffTheRecordProfile()
          : nullptr;
  for (auto* target_browser : *BrowserList::GetInstance()) {
    if (target_browser->profile() == profile ||
        target_browser->profile() == incognito_profile) {
      TabStripModel* target_tab_strip = target_browser->tab_strip_model();
      for (int i = 0; i < target_tab_strip->count(); ++i) {
        content::WebContents* target_contents =
            target_tab_strip->GetWebContentsAt(i);
        if (sessions::SessionTabHelper::IdForTab(target_contents).id() ==
            tab_id) {
          return target_contents;
        }
      }
    }
  }
  return nullptr;
}

MediaRouteProviderId FixProviderId(MediaRouteProviderId provider_id) {
  // This is a hack to ensure the extension handles the CreateRoute call until
  // the CastMediaRouteProvider supports it.
  // TODO(crbug.com/698940): Remove check for Cast when CastMediaRouteProvider
  // supports route management.
  // TODO(https://crbug.com/808720): Remove check for DIAL when in-browser DIAL
  // MRP is fully implemented.
  if ((provider_id == MediaRouteProviderId::CAST &&
       !CastMediaRouteProviderEnabled()) ||
      (provider_id == MediaRouteProviderId::DIAL &&
       !DialMediaRouteProviderEnabled())) {
    return MediaRouteProviderId::EXTENSION;
  }
  return provider_id;
}

DesktopMediaPickerController::Params MakeDesktopPickerParams(
    content::WebContents* web_contents) {
#ifndef OS_CHROMEOS
  DCHECK(web_contents);
#endif

  DesktopMediaPickerController::Params params;
  // Value of |web_contents| comes from the UI, and typically corresponds to
  // the active tab.
  params.web_contents = web_contents;
  if (web_contents)
    params.context = web_contents->GetTopLevelNativeWindow();
  params.app_name = l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  params.target_name = params.app_name;
  params.select_only_screen = true;
  return params;
}

}  // namespace

using SinkAvailability = mojom::MediaRouter::SinkAvailability;

MediaRouterMojoImpl::MediaRoutesQuery::MediaRoutesQuery() = default;

MediaRouterMojoImpl::MediaRoutesQuery::~MediaRoutesQuery() = default;

MediaRouterMojoImpl::MediaSinksQuery::MediaSinksQuery() = default;

MediaRouterMojoImpl::MediaSinksQuery::~MediaSinksQuery() = default;

MediaRouterMojoImpl::MediaRouterMojoImpl(content::BrowserContext* context)
    : instance_id_(base::GenerateGUID()), context_(context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MediaRouterMojoImpl::~MediaRouterMojoImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void MediaRouterMojoImpl::RegisterMediaRouteProvider(
    MediaRouteProviderId provider_id,
    mojo::PendingRemote<mojom::MediaRouteProvider> media_route_provider_remote,
    mojom::MediaRouter::RegisterMediaRouteProviderCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!base::Contains(media_route_providers_, provider_id));
  mojo::Remote<mojom::MediaRouteProvider> bound_remote(
      std::move(media_route_provider_remote));
  bound_remote.set_disconnect_handler(
      base::BindOnce(&MediaRouterMojoImpl::OnProviderConnectionError,
                     weak_factory_.GetWeakPtr(), provider_id));
  media_route_providers_[provider_id] = std::move(bound_remote);
  std::move(callback).Run(instance_id_, mojom::MediaRouteProviderConfig::New());
}

void MediaRouterMojoImpl::OnIssue(const IssueInfo& issue) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG_WITH_INSTANCE(1) << "OnIssue " << issue.title;
  GetIssueManager()->AddIssue(issue);
}

void MediaRouterMojoImpl::OnSinksReceived(
    MediaRouteProviderId provider_id,
    const std::string& media_source,
    const std::vector<MediaSinkInternal>& internal_sinks,
    const std::vector<url::Origin>& origins) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG_WITH_INSTANCE(1) << "OnSinksReceived";
  auto it = sinks_queries_.find(media_source);
  if (it == sinks_queries_.end()) {
    DVLOG_WITH_INSTANCE(1) << "Received sink list without MediaSinksQuery.";
    return;
  }

  std::vector<MediaSink> sinks;
  sinks.reserve(internal_sinks.size());
  for (const auto& internal_sink : internal_sinks)
    sinks.push_back(internal_sink.sink());

  auto* sinks_query = it->second.get();
  sinks_query->SetSinksForProvider(provider_id, sinks);
  sinks_query->set_origins(origins);
  sinks_query->NotifyObservers();
}

void MediaRouterMojoImpl::OnRoutesUpdated(
    MediaRouteProviderId provider_id,
    const std::vector<MediaRoute>& routes,
    const std::string& media_source,
    const std::vector<std::string>& joinable_route_ids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG_WITH_INSTANCE(1) << "OnRoutesUpdated";
  auto it = routes_queries_.find(media_source);
  if (it == routes_queries_.end() || !it->second->HasObservers()) {
    DVLOG_WITH_INSTANCE(1)
        << "Received route list without any active observers: " << media_source;
    return;
  }

  auto* routes_query = it->second.get();
  routes_query->SetRoutesForProvider(provider_id, routes, joinable_route_ids);
  routes_query->NotifyObservers();
}

void MediaRouterMojoImpl::RouteResponseReceived(
    const std::string& presentation_id,
    MediaRouteProviderId provider_id,
    bool is_incognito,
    MediaRouteResponseCallback callback,
    bool is_join,
    const base::Optional<MediaRoute>& media_route,
    mojom::RoutePresentationConnectionPtr connection,
    const base::Optional<std::string>& error_text,
    RouteRequestResult::ResultCode result_code) {
  DCHECK(!connection ||
         (connection->connection_remote && connection->connection_receiver));
  std::unique_ptr<RouteRequestResult> result;
  if (!media_route) {
    // An error occurred.
    const std::string& error = (error_text && !error_text->empty())
                                   ? *error_text
                                   : std::string("Unknown error.");
    result = RouteRequestResult::FromError(error, result_code);
  } else if (media_route->is_incognito() != is_incognito) {
    std::string error = base::StringPrintf(
        "Mismatch in incognito status: request = %d, response = %d",
        is_incognito, media_route->is_incognito());
    result = RouteRequestResult::FromError(
        error, RouteRequestResult::INCOGNITO_MISMATCH);
  } else {
    result = RouteRequestResult::FromSuccess(*media_route, presentation_id);
    OnRouteAdded(provider_id, *media_route);
  }

  if (is_join) {
    MediaRouterMojoMetrics::RecordJoinRouteResultCode(provider_id,
                                                      result->result_code());
  } else {
    MediaRouterMojoMetrics::RecordCreateRouteResultCode(provider_id,
                                                        result->result_code());
  }

  std::move(callback).Run(std::move(connection), *result);
}

void MediaRouterMojoImpl::CreateRoute(const MediaSource::Id& source_id,
                                      const MediaSink::Id& sink_id,
                                      const url::Origin& origin,
                                      content::WebContents* web_contents,
                                      MediaRouteResponseCallback callback,
                                      base::TimeDelta timeout,
                                      bool incognito) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(callback);
  const MediaSink* sink = GetSinkById(sink_id);
  if (!sink) {
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Sink not found", RouteRequestResult::SINK_NOT_FOUND);
    MediaRouterMojoMetrics::RecordCreateRouteResultCode(
        MediaRouteProviderId::UNKNOWN, result->result_code());
    std::move(callback).Run(nullptr, *result);
    return;
  }

  const MediaSource source(source_id);
  if (source.IsTabMirroringSource()) {
    // Ensure the CastRemotingConnector is created before mirroring starts.
    CastRemotingConnector* const connector =
        CastRemotingConnector::Get(web_contents);
    connector->ResetRemotingPermission();

    RecordTabMirroringMetrics(web_contents);
  }

  MediaRouterMetrics::RecordMediaSinkType(sink->icon_type());
  const MediaRouteProviderId provider_id = FixProviderId(sink->provider_id());

  const std::string presentation_id = MediaRouterBase::CreatePresentationId();
  auto mr_callback = base::BindOnce(
      &MediaRouterMojoImpl::RouteResponseReceived, weak_factory_.GetWeakPtr(),
      presentation_id, provider_id, incognito, std::move(callback), false);

  if (source.IsDesktopMirroringSource() &&
      // This check is because extension-based MRPs are responsible for showing
      // the dialog themselves if they support desktop casting.
      provider_id != MediaRouteProviderId::EXTENSION) {
    desktop_picker_.Show(
        MakeDesktopPickerParams(web_contents),
        {content::DesktopMediaID::TYPE_SCREEN},
        base::BindOnce(&MediaRouterMojoImpl::CreateRouteWithSelectedDesktop,
                       weak_factory_.GetWeakPtr(), provider_id, sink_id,
                       presentation_id, origin, web_contents, timeout,
                       incognito, std::move(mr_callback)));
  } else {
    // Previously the tab ID was set to -1 for non-mirroring sessions, which
    // mostly works, but it breaks auto-joining.
    const int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
    media_route_providers_[provider_id]->CreateRoute(
        source_id, sink_id, presentation_id, origin, tab_id, timeout, incognito,
        std::move(mr_callback));
  }
}

void MediaRouterMojoImpl::JoinRoute(const MediaSource::Id& source_id,
                                    const std::string& presentation_id,
                                    const url::Origin& origin,
                                    content::WebContents* web_contents,
                                    MediaRouteResponseCallback callback,
                                    base::TimeDelta timeout,
                                    bool incognito) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Optional<MediaRouteProviderId> provider_id =
      GetProviderIdForPresentation(presentation_id);
  if (!provider_id || !HasJoinableRoute()) {
    DVLOG_WITH_INSTANCE(1) << "Cannot join route with source: " << source_id
                           << " and presentation ID: " << presentation_id;
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Route not found", RouteRequestResult::ROUTE_NOT_FOUND);
    MediaRouterMojoMetrics::RecordJoinRouteResultCode(
        provider_id.value_or(MediaRouteProviderId::UNKNOWN),
        result->result_code());
    // TODO(btolsch): This should really move |result| now that there's only a
    // single callback.
    std::move(callback).Run(nullptr, *result);
    return;
  }

  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  auto mr_callback = base::BindOnce(
      &MediaRouterMojoImpl::RouteResponseReceived, weak_factory_.GetWeakPtr(),
      presentation_id, *provider_id, incognito, std::move(callback), true);
  media_route_providers_[*provider_id]->JoinRoute(
      source_id, presentation_id, origin, tab_id, timeout, incognito,
      std::move(mr_callback));
}

void MediaRouterMojoImpl::ConnectRouteByRouteId(
    const MediaSource::Id& source_id,
    const MediaRoute::Id& route_id,
    const url::Origin& origin,
    content::WebContents* web_contents,
    MediaRouteResponseCallback callback,
    base::TimeDelta timeout,
    bool incognito) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Optional<MediaRouteProviderId> provider_id =
      GetProviderIdForRoute(route_id);
  if (!provider_id) {
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Route not found", RouteRequestResult::ROUTE_NOT_FOUND);
    std::move(callback).Run(nullptr, *result);
    return;
  }

  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  std::string presentation_id = MediaRouterBase::CreatePresentationId();
  auto mr_callback = base::BindOnce(
      &MediaRouterMojoImpl::RouteResponseReceived, weak_factory_.GetWeakPtr(),
      presentation_id, *provider_id, incognito, std::move(callback), true);
  media_route_providers_[*provider_id]->ConnectRouteByRouteId(
      source_id, route_id, presentation_id, origin, tab_id, timeout, incognito,
      std::move(mr_callback));
}

void MediaRouterMojoImpl::TerminateRoute(const MediaRoute::Id& route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Optional<MediaRouteProviderId> provider_id =
      GetProviderIdForRoute(route_id);
  if (!provider_id) {
    DVLOG_WITH_INSTANCE(1) << __func__ << ": route not found: " << route_id;
    MediaRouterMojoMetrics::RecordJoinRouteResultCode(
        MediaRouteProviderId::UNKNOWN, RouteRequestResult::ROUTE_NOT_FOUND);
    return;
  }

  DVLOG(2) << "TerminateRoute " << route_id;
  auto callback =
      base::BindOnce(&MediaRouterMojoImpl::OnTerminateRouteResult,
                     weak_factory_.GetWeakPtr(), route_id, *provider_id);
  media_route_providers_[*provider_id]->TerminateRoute(route_id,
                                                       std::move(callback));
}

void MediaRouterMojoImpl::DetachRoute(const MediaRoute::Id& route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Optional<MediaRouteProviderId> provider_id =
      GetProviderIdForRoute(route_id);
  if (!provider_id) {
    DVLOG_WITH_INSTANCE(1) << __func__ << ": route not found: " << route_id;
    return;
  }

  media_route_providers_[*provider_id]->DetachRoute(route_id);
}

void MediaRouterMojoImpl::SendRouteMessage(const MediaRoute::Id& route_id,
                                           const std::string& message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Optional<MediaRouteProviderId> provider_id =
      GetProviderIdForRoute(route_id);
  if (!provider_id) {
    DVLOG_WITH_INSTANCE(1) << __func__ << ": route not found: " << route_id;
    return;
  }

  media_route_providers_[*provider_id]->SendRouteMessage(route_id, message);
}

void MediaRouterMojoImpl::SendRouteBinaryMessage(
    const MediaRoute::Id& route_id,
    std::unique_ptr<std::vector<uint8_t>> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Optional<MediaRouteProviderId> provider_id =
      GetProviderIdForRoute(route_id);
  if (!provider_id) {
    DVLOG_WITH_INSTANCE(1) << __func__ << ": route not found: " << route_id;
    return;
  }

  media_route_providers_[*provider_id]->SendRouteBinaryMessage(route_id, *data);
}

void MediaRouterMojoImpl::OnUserGesture() {}

void MediaRouterMojoImpl::GetMediaController(
    const MediaRoute::Id& route_id,
    mojo::PendingReceiver<mojom::MediaController> controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {
  auto* route = GetRoute(route_id);
  base::Optional<MediaRouteProviderId> provider_id =
      GetProviderIdForRoute(route_id);
  if (!route || !provider_id) {
    DVLOG_WITH_INSTANCE(1) << __func__ << ": route not found: " << route_id;
    return;
  }
  if (route->controller_type() == RouteControllerType::kNone) {
    DVLOG_WITH_INSTANCE(1) << __func__
                           << ": route does not support controller: "
                           << route_id;
    return;
  }
  auto callback = base::BindOnce(&MediaRouterMojoImpl::OnMediaControllerCreated,
                                 weak_factory_.GetWeakPtr(), route_id);
  media_route_providers_[*provider_id]->CreateMediaRouteController(
      route_id, std::move(controller), std::move(observer),
      std::move(callback));
}

void MediaRouterMojoImpl::MediaSinksQuery::SetSinksForProvider(
    MediaRouteProviderId provider_id,
    const std::vector<MediaSink>& sinks) {
  base::EraseIf(cached_sink_list_, [&provider_id](const MediaSink& sink) {
    return sink.provider_id() == provider_id;
  });
  cached_sink_list_.insert(cached_sink_list_.end(), sinks.begin(), sinks.end());
}

void MediaRouterMojoImpl::MediaSinksQuery::Reset() {
  cached_sink_list_.clear();
  origins_.clear();
}

void MediaRouterMojoImpl::MediaSinksQuery::AddObserver(
    MediaSinksObserver* observer) {
  observers_.AddObserver(observer);
  observer->OnSinksUpdated(cached_sink_list_, origins_);
}

void MediaRouterMojoImpl::MediaSinksQuery::RemoveObserver(
    MediaSinksObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaRouterMojoImpl::MediaSinksQuery::NotifyObservers() {
  for (auto& observer : observers_)
    observer.OnSinksUpdated(cached_sink_list_, origins_);
}

bool MediaRouterMojoImpl::MediaSinksQuery::HasObserver(
    MediaSinksObserver* observer) const {
  return observers_.HasObserver(observer);
}

bool MediaRouterMojoImpl::MediaSinksQuery::HasObservers() const {
  return observers_.might_have_observers();
}

void MediaRouterMojoImpl::MediaRoutesQuery::SetRoutesForProvider(
    MediaRouteProviderId provider_id,
    const std::vector<MediaRoute>& routes,
    const std::vector<MediaRoute::Id>& joinable_route_ids) {
  providers_to_routes_[provider_id] = routes;
  UpdateCachedRouteList();

  providers_to_joinable_routes_[provider_id] = joinable_route_ids;
  joinable_route_ids_.clear();
  for (const auto& provider_to_joinable_routes :
       providers_to_joinable_routes_) {
    joinable_route_ids_.insert(joinable_route_ids_.end(),
                               provider_to_joinable_routes.second.begin(),
                               provider_to_joinable_routes.second.end());
  }
}

bool MediaRouterMojoImpl::MediaRoutesQuery::AddRouteForProvider(
    MediaRouteProviderId provider_id,
    const MediaRoute& route) {
  std::vector<MediaRoute>& routes = providers_to_routes_[provider_id];
  if (std::find_if(routes.begin(), routes.end(),
                   [&route](const MediaRoute& existing_route) {
                     return existing_route.media_route_id() ==
                            route.media_route_id();
                   }) == routes.end()) {
    routes.push_back(route);
    UpdateCachedRouteList();
    return true;
  }
  return false;
}

void MediaRouterMojoImpl::MediaRoutesQuery::UpdateCachedRouteList() {
  cached_route_list_.emplace();
  for (const auto& provider_to_routes : providers_to_routes_) {
    cached_route_list_->insert(cached_route_list_->end(),
                               provider_to_routes.second.begin(),
                               provider_to_routes.second.end());
  }
}

void MediaRouterMojoImpl::MediaRoutesQuery::AddObserver(
    MediaRoutesObserver* observer) {
  observers_.AddObserver(observer);
  observer->OnRoutesUpdated(
      cached_route_list_.value_or(std::vector<MediaRoute>()),
      joinable_route_ids_);
}

void MediaRouterMojoImpl::MediaRoutesQuery::RemoveObserver(
    MediaRoutesObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaRouterMojoImpl::MediaRoutesQuery::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnRoutesUpdated(
        cached_route_list_.value_or(std::vector<MediaRoute>()),
        joinable_route_ids_);
  }
}

bool MediaRouterMojoImpl::MediaRoutesQuery::HasObserver(
    MediaRoutesObserver* observer) const {
  return observers_.HasObserver(observer);
}

bool MediaRouterMojoImpl::MediaRoutesQuery::HasObservers() const {
  return observers_.might_have_observers();
}

MediaRouterMojoImpl::ProviderSinkAvailability::ProviderSinkAvailability() =
    default;

MediaRouterMojoImpl::ProviderSinkAvailability::~ProviderSinkAvailability() =
    default;

bool MediaRouterMojoImpl::ProviderSinkAvailability::SetAvailabilityForProvider(
    MediaRouteProviderId provider_id,
    SinkAvailability availability) {
  SinkAvailability previous_availability = SinkAvailability::UNAVAILABLE;
  const auto& availability_for_provider = availabilities_.find(provider_id);
  if (availability_for_provider != availabilities_.end()) {
    previous_availability = availability_for_provider->second;
  }
  availabilities_[provider_id] = availability;
  if (availability == previous_availability) {
    return false;
  } else {
    UpdateOverallAvailability();
    return true;
  }
}

bool MediaRouterMojoImpl::ProviderSinkAvailability::IsAvailableForProvider(
    MediaRouteProviderId provider_id) const {
  const auto& it = availabilities_.find(provider_id);
  return it == availabilities_.end()
             ? false
             : it->second != SinkAvailability::UNAVAILABLE;
}

bool MediaRouterMojoImpl::ProviderSinkAvailability::IsAvailable() const {
  return overall_availability_ != SinkAvailability::UNAVAILABLE;
}

void MediaRouterMojoImpl::ProviderSinkAvailability::
    UpdateOverallAvailability() {
  overall_availability_ = SinkAvailability::UNAVAILABLE;
  for (const auto& availability : availabilities_) {
    switch (availability.second) {
      case SinkAvailability::UNAVAILABLE:
        break;
      case SinkAvailability::PER_SOURCE:
        overall_availability_ = SinkAvailability::PER_SOURCE;
        break;
      case SinkAvailability::AVAILABLE:
        overall_availability_ = SinkAvailability::AVAILABLE;
        return;
    }
  }
}

bool MediaRouterMojoImpl::RegisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Create an observer list for the media source and add |observer|
  // to it. Fail if |observer| is already registered.
  const std::string& source_id =
      observer->source() ? observer->source()->id() : "";
  std::unique_ptr<MediaSinksQuery>& sinks_query = sinks_queries_[source_id];
  bool is_new_query = false;
  if (!sinks_query) {
    is_new_query = true;
    sinks_query = std::make_unique<MediaSinksQuery>();
  } else {
    DCHECK(!sinks_query->HasObserver(observer));
  }
  sinks_query->AddObserver(observer);

  // If sink availability is UNAVAILABLE or the query isn't new, then there is
  // no need to call MRPs.
  if (is_new_query) {
    for (const auto& provider : media_route_providers_) {
      if (sink_availability_.IsAvailableForProvider(provider.first)) {
        provider.second->StartObservingMediaSinks(source_id);
      }
    }
  }
  return true;
}

void MediaRouterMojoImpl::UnregisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const std::string& source_id =
      observer->source() ? observer->source()->id() : "";
  auto it = sinks_queries_.find(source_id);
  if (it == sinks_queries_.end() || !it->second->HasObserver(observer))
    return;

  // If we are removing the final observer for the source, then stop
  // observing sinks for it.
  // HasObservers() is reliable here on the assumption that this call
  // is not inside the ObserverList iteration.
  it->second->RemoveObserver(observer);
  if (!it->second->HasObservers()) {
    // Only ask MRPs to stop observing media sinks if there are sinks available.
    // Otherwise, the MRPs would have discarded the queries already.
    for (const auto& provider : media_route_providers_) {
      if (sink_availability_.IsAvailableForProvider(provider.first)) {
        provider.second->StopObservingMediaSinks(source_id);
      }
    }
    sinks_queries_.erase(source_id);
  }
}

void MediaRouterMojoImpl::RegisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const MediaSource::Id source_id = observer->source_id();
  auto& routes_query = routes_queries_[source_id];
  bool is_new_query = false;
  if (!routes_query) {
    is_new_query = true;
    routes_query = std::make_unique<MediaRoutesQuery>();
  } else {
    DCHECK(!routes_query->HasObserver(observer));
  }

  routes_query->AddObserver(observer);
  if (is_new_query) {
    for (const auto& provider : media_route_providers_)
      provider.second->StartObservingMediaRoutes(source_id);
    // The MRPs will call MediaRouterMojoImpl::OnRoutesUpdated() soon, if there
    // are any existing routes the new observer should be aware of.
  } else if (routes_query->cached_route_list()) {
    // Return to the event loop before notifying of a cached route list because
    // MediaRoutesObserver is calling this method from its constructor, and that
    // must complete before invoking its virtual OnRoutesUpdated() method.
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&MediaRouterMojoImpl::NotifyOfExistingRoutesIfRegistered,
                       weak_factory_.GetWeakPtr(), source_id, observer));
  }
}

void MediaRouterMojoImpl::NotifyOfExistingRoutesIfRegistered(
    const MediaSource::Id& source_id,
    MediaRoutesObserver* observer) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check that the route query still exists with a cached result, and that the
  // observer is still registered. Otherwise, there is nothing to report to the
  // observer.
  const auto it = routes_queries_.find(source_id);
  if (it == routes_queries_.end() || !it->second->cached_route_list() ||
      !it->second->HasObserver(observer)) {
    return;
  }

  observer->OnRoutesUpdated(*it->second->cached_route_list(),
                            it->second->joinable_route_ids());
}

void MediaRouterMojoImpl::UnregisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  const MediaSource::Id source_id = observer->source_id();
  auto it = routes_queries_.find(source_id);
  if (it == routes_queries_.end() || !it->second->HasObserver(observer)) {
    return;
  }

  // If we are removing the final observer for the source, then stop
  // observing routes for it.
  // HasObservers() is reliable here on the assumption that this call
  // is not inside the ObserverList iteration.
  it->second->RemoveObserver(observer);
  if (!it->second->HasObservers()) {
    for (const auto& provider : media_route_providers_)
      provider.second->StopObservingMediaRoutes(source_id);
    routes_queries_.erase(source_id);
  }
}

void MediaRouterMojoImpl::RegisterRouteMessageObserver(
    RouteMessageObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  const MediaRoute::Id& route_id = observer->route_id();
  auto& observer_list = message_observers_[route_id];
  if (!observer_list) {
    observer_list = std::make_unique<RouteMessageObserverList>();
  } else {
    DCHECK(!observer_list->HasObserver(observer));
  }

  bool should_listen = !observer_list->might_have_observers();
  observer_list->AddObserver(observer);
  if (should_listen) {
    base::Optional<MediaRouteProviderId> provider_id =
        GetProviderIdForRoute(route_id);
    if (provider_id) {
      media_route_providers_[*provider_id]->StartListeningForRouteMessages(
          route_id);
    }
  }
}

void MediaRouterMojoImpl::UnregisterRouteMessageObserver(
    RouteMessageObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);

  const MediaRoute::Id& route_id = observer->route_id();
  auto it = message_observers_.find(route_id);
  if (it == message_observers_.end() || !it->second->HasObserver(observer))
    return;

  it->second->RemoveObserver(observer);
  if (!it->second->might_have_observers()) {
    message_observers_.erase(route_id);
    base::Optional<MediaRouteProviderId> provider_id =
        GetProviderIdForRoute(route_id);
    if (provider_id) {
      media_route_providers_[*provider_id]->StopListeningForRouteMessages(
          route_id);
    }
  }
}

void MediaRouterMojoImpl::OnRouteMessagesReceived(
    const std::string& route_id,
    std::vector<mojom::RouteMessagePtr> messages) {
  DVLOG_WITH_INSTANCE(1) << "OnRouteMessagesReceived";

  if (messages.empty())
    return;

  auto it = message_observers_.find(route_id);
  if (it == message_observers_.end())
    return;

  for (auto& observer : *it->second) {
    // TODO(mfoltz): We have to clone the messages here in case there are
    // multiple observers.  This can be removed once we stop passing messages
    // through the MR and use the PresentationConnectionPtr directly.
    std::vector<mojom::RouteMessagePtr> messages_copy;
    for (auto& message : messages)
      messages_copy.emplace_back(message->Clone());

    observer.OnMessagesReceived(std::move(messages_copy));
  }
}

void MediaRouterMojoImpl::OnSinkAvailabilityUpdated(
    MediaRouteProviderId provider_id,
    SinkAvailability availability) {
  if (!sink_availability_.SetAvailabilityForProvider(provider_id, availability))
    return;

  if (availability != SinkAvailability::UNAVAILABLE) {
    // Sinks are now available. Tell the MRP to start all sink queries again.
    auto& provider = media_route_providers_[provider_id];
    for (const auto& source_and_query : sinks_queries_)
      provider->StartObservingMediaSinks(source_and_query.first);
  } else if (!sink_availability_.IsAvailable()) {
    // Sinks are no longer available. MRPs have already removed all sink
    // queries.
    for (auto& source_and_query : sinks_queries_)
      source_and_query.second->Reset();
  }
}

void MediaRouterMojoImpl::OnPresentationConnectionStateChanged(
    const std::string& route_id,
    media_router::mojom::MediaRouter::PresentationConnectionState state) {
  NotifyPresentationConnectionStateChange(
      route_id, PresentationConnectionStateToBlink(state));
}

void MediaRouterMojoImpl::OnPresentationConnectionClosed(
    const std::string& route_id,
    media_router::mojom::MediaRouter::PresentationConnectionCloseReason reason,
    const std::string& message) {
  NotifyPresentationConnectionClose(
      route_id, PresentationConnectionCloseReasonToBlink(reason), message);
}

void MediaRouterMojoImpl::OnTerminateRouteResult(
    const MediaRoute::Id& route_id,
    MediaRouteProviderId provider_id,
    const base::Optional<std::string>& error_text,
    RouteRequestResult::ResultCode result_code) {
  if (result_code != RouteRequestResult::OK) {
    LOG(WARNING) << "Failed to terminate route " << route_id
                 << ": result_code = " << result_code << ", "
                 << error_text.value_or(std::string());
  }
  MediaRouterMojoMetrics::RecordMediaRouteProviderTerminateRoute(provider_id,
                                                                 result_code);
}

void MediaRouterMojoImpl::OnRouteAdded(MediaRouteProviderId provider_id,
                                       const MediaRoute& route) {
  for (auto& routes_query : routes_queries_) {
    if (routes_query.second->AddRouteForProvider(provider_id, route))
      routes_query.second->NotifyObservers();
  }
}

void MediaRouterMojoImpl::SyncStateToMediaRouteProvider(
    MediaRouteProviderId provider_id) {
  const auto& provider = media_route_providers_[provider_id];
  // Sink queries.
  if (sink_availability_.IsAvailableForProvider(provider_id)) {
    for (const auto& it : sinks_queries_)
      provider->StartObservingMediaSinks(it.first);
  }

  // Route queries.
  for (const auto& it : routes_queries_)
    provider->StartObservingMediaRoutes(it.first);

  // Route messages.
  for (const auto& it : message_observers_)
    provider->StartListeningForRouteMessages(it.first);
}

void MediaRouterMojoImpl::UpdateMediaSinks(const MediaSource::Id& source_id) {
  for (const auto& provider : media_route_providers_)
    provider.second->UpdateMediaSinks(source_id);
}

void MediaRouterMojoImpl::OnMediaControllerCreated(
    const MediaRoute::Id& route_id,
    bool success) {
  DVLOG_WITH_INSTANCE(1) << "OnMediaControllerCreated: " << route_id
                         << (success ? " was successful." : " failed.");
  MediaRouterMojoMetrics::RecordMediaRouteControllerCreationResult(success);
}

void MediaRouterMojoImpl::OnProviderConnectionError(
    MediaRouteProviderId provider_id) {
  media_route_providers_.erase(provider_id);
}

void MediaRouterMojoImpl::OnMediaRemoterCreated(
    int32_t tab_id,
    mojo::PendingRemote<media::mojom::MirrorServiceRemoter> remoter,
    mojo::PendingReceiver<media::mojom::MirrorServiceRemotingSource>
        source_receiver) {
  DVLOG_WITH_INSTANCE(1) << __func__ << ": tab_id = " << tab_id;

  auto it = remoting_sources_.find(SessionID::FromSerializedValue(tab_id));
  if (it == remoting_sources_.end()) {
    LOG(WARNING) << __func__
                 << ": No registered remoting source for tab_id = " << tab_id;
    return;
  }

  CastRemotingConnector* connector = it->second;
  connector->ConnectToService(std::move(source_receiver), std::move(remoter));
}

void MediaRouterMojoImpl::GetMediaSinkServiceStatus(
    mojom::MediaRouter::GetMediaSinkServiceStatusCallback callback) {
  MediaSinkServiceStatus status;
  std::move(callback).Run(status.GetStatusAsJSONString());
}

void MediaRouterMojoImpl::GetMirroringServiceHostForTab(
    int32_t target_tab_id,
    mojo::PendingReceiver<mirroring::mojom::MirroringServiceHost> receiver) {
  mirroring::CastMirroringServiceHost::GetForTab(
      GetWebContentsFromId(target_tab_id, context_,
                           true /* include_incognito */),
      std::move(receiver));
}

// TODO(crbug.com/809249): This method is currently part of a Mojo interface,
// but eventually it won't be.  When that happens, change the sigature so it can
// report errors.  Also remove the |initiator_tab_id| parameter.
void MediaRouterMojoImpl::GetMirroringServiceHostForDesktop(
    int32_t initiator_tab_id,
    const std::string& desktop_stream_id,
    mojo::PendingReceiver<mirroring::mojom::MirroringServiceHost> receiver) {
  if (CastMediaRouteProviderEnabled()) {
    if (!pending_stream_request_ ||
        pending_stream_request_->stream_id != desktop_stream_id) {
      DLOG(ERROR) << "Pending request for stream not found: "
                  << desktop_stream_id;
      return;
    }
    const PendingStreamRequest& request = *pending_stream_request_;
    const auto media_id =
        content::DesktopStreamsRegistry::GetInstance()->RequestMediaForStreamId(
            request.stream_id, request.render_process_id,
            request.render_frame_id, request.origin, nullptr,
            content::kRegistryStreamTypeDesktop);
    if (media_id.is_null()) {
      DLOG(ERROR) << "Failed to find media for stream ID " << desktop_stream_id;
      return;
    }
    mirroring::CastMirroringServiceHost::GetForDesktop(media_id,
                                                       std::move(receiver));
  } else {
    // This code path is taken when the mirroring service is enabled
    // but the native Cast MRP is not.
    //
    // TODO(crbug.com/974335): Remove this code once we fully launch the native
    // Cast Media Route Provider.
    mirroring::CastMirroringServiceHost::GetForDesktop(
        EventPageRequestManagerFactory::GetApiForBrowserContext(context_)
            ->GetEventPageWebContents(),
        desktop_stream_id, std::move(receiver));
  }
}

void MediaRouterMojoImpl::GetMirroringServiceHostForOffscreenTab(
    const GURL& presentation_url,
    const std::string& presentation_id,
    mojo::PendingReceiver<mirroring::mojom::MirroringServiceHost> receiver) {
  if (IsValidPresentationUrl(presentation_url)) {
    mirroring::CastMirroringServiceHost::GetForOffscreenTab(
        context_, presentation_url, presentation_id, std::move(receiver));
  }
}

void MediaRouterMojoImpl::BindToMojoReceiver(
    mojo::PendingReceiver<mojom::MediaRouter> receiver) {
  receivers_.Add(this, std::move(receiver));
}

base::Optional<MediaRouteProviderId> MediaRouterMojoImpl::GetProviderIdForRoute(
    const MediaRoute::Id& route_id) {
  for (const auto& routes_query : routes_queries_) {
    MediaRoutesQuery* query = routes_query.second.get();
    for (const auto& provider_to_routes : query->providers_to_routes()) {
      const MediaRouteProviderId provider_id = provider_to_routes.first;
      const std::vector<MediaRoute>& routes = provider_to_routes.second;
      if (std::find_if(routes.begin(), routes.end(),
                       [&route_id](const MediaRoute& route) {
                         return route.media_route_id() == route_id;
                       }) != routes.end()) {
        return provider_id;
      }
    }
  }
  return base::nullopt;
}

base::Optional<MediaRouteProviderId> MediaRouterMojoImpl::GetProviderIdForSink(
    const MediaSink::Id& sink_id) {
  const MediaSink* sink = GetSinkById(sink_id);
  return sink ? base::make_optional<MediaRouteProviderId>(sink->provider_id())
              : base::nullopt;
}

base::Optional<MediaRouteProviderId>
MediaRouterMojoImpl::GetProviderIdForPresentation(
    const std::string& presentation_id) {
  for (const auto& routes_query : routes_queries_) {
    MediaRoutesQuery* query = routes_query.second.get();
    for (const auto& provider_to_routes : query->providers_to_routes()) {
      const MediaRouteProviderId provider_id = provider_to_routes.first;
      const std::vector<MediaRoute>& routes = provider_to_routes.second;
      auto pred = [&presentation_id](const MediaRoute& route) {
        return route.presentation_id() == presentation_id;
      };
      DCHECK_LE(std::count_if(routes.begin(), routes.end(), pred), 1)
          << "Found multiple routes for presentation ID " << presentation_id;
      if (std::find_if(routes.begin(), routes.end(), pred) != routes.end()) {
        return provider_id;
      }
    }
  }
  return base::nullopt;
}

const MediaSink* MediaRouterMojoImpl::GetSinkById(
    const MediaSink::Id& sink_id) const {
  // TODO(takumif): It is inefficient to iterate through all the sinks queries,
  // so there should be one list containing all the sinks.
  for (const auto& sinks_query : sinks_queries_) {
    const std::vector<MediaSink>& sinks =
        sinks_query.second->cached_sink_list();
    auto pred = [&sink_id](const MediaSink& sink) {
      return sink.id() == sink_id;
    };
    DCHECK_LE(std::count_if(sinks.begin(), sinks.end(), pred), 1)
        << "Found multiple sinks with ID " << sink_id;
    auto sink_it = std::find_if(sinks.begin(), sinks.end(), pred);
    if (sink_it != sinks.end())
      return &(*sink_it);
  }
  return nullptr;
}

void MediaRouterMojoImpl::CreateRouteWithSelectedDesktop(
    MediaRouteProviderId provider_id,
    const std::string& sink_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    content::WebContents* web_contents,
    base::TimeDelta timeout,
    bool incognito,
    mojom::MediaRouteProvider::CreateRouteCallback mr_callback,
    const std::string& err,
    content::DesktopMediaID media_id) {
  if (!err.empty()) {
    std::move(mr_callback)
        .Run(base::nullopt, nullptr, err,
             RouteRequestResult::DESKTOP_PICKER_FAILED);
    return;
  }

  if (media_id.is_null()) {
    std::move(mr_callback)
        .Run(base::nullopt, nullptr, "User canceled capture dialog",
             RouteRequestResult::CANCELLED);
    return;
  }

  // TODO(jrw): This is kind of ridiculous.  The PendingStreamRequest struct
  // only exists to store the arguments given to
  // DesktopStreamsRegistry::RegisterStream() so they can later be passed back
  // to DesktopStreamsRegistry::RequestMediaForStreamId(), but the saved values
  // aren't actually needed in RequestMediaForStreamId() except to prove that
  // the request is legitimate.  Creating a more lenient version of the methods
  // in DesktopStreamsRegistry, or simply storing |media_id| directly, is likely
  // a better solution, but the security implications aren't entirely clear to
  // me, so for now I'm going with a clumsy solution that works and doesn't
  // require altering DesktopStreamsRegistry.
  DCHECK(!pending_stream_request_);
  pending_stream_request_.emplace();
  PendingStreamRequest& request = *pending_stream_request_;
#ifndef OS_CHROMEOS
  DCHECK(web_contents);
  content::RenderFrameHost* const main_frame = web_contents->GetMainFrame();
  request.render_process_id = main_frame->GetProcess()->GetID();
  request.render_frame_id = main_frame->GetRoutingID();
  request.origin = url::Origin::Create(web_contents->GetVisibleURL());
#endif
  request.stream_id =
      content::DesktopStreamsRegistry::GetInstance()->RegisterStream(
          request.render_process_id, request.render_frame_id, request.origin,
          media_id, "ChromeMediaRouter", content::kRegistryStreamTypeDesktop);

  media_route_providers_[provider_id]->CreateRoute(
      MediaSource::ForDesktop(request.stream_id).id(), sink_id, presentation_id,
      origin, -1, timeout, incognito,
      base::BindOnce(
          [](mojom::MediaRouteProvider::CreateRouteCallback inner_callback,
             base::WeakPtr<MediaRouterMojoImpl> self,
             const std::string& stream_id,
             const base::Optional<media_router::MediaRoute>& route,
             mojom::RoutePresentationConnectionPtr connection,
             const base::Optional<std::string>& error_text,
             RouteRequestResult::ResultCode result_code) {
            if (self)
              self->pending_stream_request_.reset();
            std::move(inner_callback)
                .Run(route, std::move(connection), error_text, result_code);
          },
          std::move(mr_callback), weak_factory_.GetWeakPtr(),
          request.stream_id));
}

void MediaRouterMojoImpl::RecordTabMirroringMetrics(
    content::WebContents* web_contents) {
  ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents);
  WebContentsAudioState audio_state = WebContentsAudioState::kWasNeverAudible;
  if (web_contents->IsCurrentlyAudible()) {
    audio_state = WebContentsAudioState::kIsCurrentlyAudible;
  } else if (web_contents->WasEverAudible()) {
    audio_state = WebContentsAudioState::kWasPreviouslyAudible;
  }

  ukm::builders::MediaRouter_TabMirroringStarted(source_id)
      .SetAudioState(static_cast<int>(audio_state))
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace media_router
