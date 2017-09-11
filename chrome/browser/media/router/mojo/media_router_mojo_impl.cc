// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_proxy.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/mojo/media_route_controller.h"
#include "chrome/browser/media/router/mojo/media_route_provider_util_win.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/media_router/media_source_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/presentation_connection_message.h"

#define DVLOG_WITH_INSTANCE(level) \
  DVLOG(level) << "MR #" << instance_id_ << ": "

#define DLOG_WITH_INSTANCE(level) DLOG(level) << "MR #" << instance_id_ << ": "

namespace media_router {

namespace {

void RunRouteRequestCallbacks(
    std::unique_ptr<RouteRequestResult> result,
    std::vector<MediaRouteResponseCallback> callbacks) {
  for (MediaRouteResponseCallback& callback : callbacks)
    std::move(callback).Run(*result);
}

}  // namespace

using SinkAvailability = mojom::MediaRouter::SinkAvailability;

MediaRouterMojoImpl::MediaRoutesQuery::MediaRoutesQuery() = default;

MediaRouterMojoImpl::MediaRoutesQuery::~MediaRoutesQuery() = default;

MediaRouterMojoImpl::MediaSinksQuery::MediaSinksQuery() = default;

MediaRouterMojoImpl::MediaSinksQuery::~MediaSinksQuery() = default;

MediaRouterMojoImpl::MediaRouterMojoImpl(content::BrowserContext* context)
    : instance_id_(base::GenerateGUID()),
      availability_(mojom::MediaRouter::SinkAvailability::UNAVAILABLE),
      context_(context),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MediaRouterMojoImpl::~MediaRouterMojoImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (dial_media_sink_service_proxy_) {
    dial_media_sink_service_proxy_->Stop();
    dial_media_sink_service_proxy_->ClearObserver(
        cast_media_sink_service_.get());
  }
  if (cast_media_sink_service_)
    cast_media_sink_service_->Stop();
}

void MediaRouterMojoImpl::OnConnectionError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  media_route_provider_.reset();
}

void MediaRouterMojoImpl::RegisterMediaRouteProvider(
    mojom::MediaRouteProviderPtr media_route_provider_ptr,
    mojom::MediaRouter::RegisterMediaRouteProviderCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  media_route_provider_ = std::move(media_route_provider_ptr);
  media_route_provider_.set_connection_error_handler(base::BindOnce(
      &MediaRouterMojoImpl::OnConnectionError, base::Unretained(this)));

  auto config = mojom::MediaRouteProviderConfig::New();
  // Enabling browser side discovery means disabling extension side discovery.
  // We are migrating discovery from the external Media Route Provider to the
  // Media Router (crbug.com/687383), so we need to disable it in the provider.
  config->enable_dial_discovery = !media_router::DialLocalDiscoveryEnabled();
  config->enable_cast_discovery = !media_router::CastDiscoveryEnabled();
  std::move(callback).Run(instance_id_, std::move(config));
  SyncStateToMediaRouteProvider();
}

void MediaRouterMojoImpl::OnIssue(const IssueInfo& issue) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG_WITH_INSTANCE(1) << "OnIssue " << issue.title;
  issue_manager_.AddIssue(issue);
}

void MediaRouterMojoImpl::OnSinksReceived(
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
  sinks_query->cached_sink_list = sinks;
  sinks_query->origins = origins;

  if (sinks_query->observers.might_have_observers()) {
    for (auto& observer : sinks_query->observers) {
      observer.OnSinksUpdated(*sinks_query->cached_sink_list,
                              sinks_query->origins);
    }
  } else {
    DVLOG_WITH_INSTANCE(1)
        << "Received sink list without any active observers: " << media_source;
  }
}

void MediaRouterMojoImpl::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes,
    const std::string& media_source,
    const std::vector<std::string>& joinable_route_ids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG_WITH_INSTANCE(1) << "OnRoutesUpdated";
  auto it = routes_queries_.find(media_source);
  if (it == routes_queries_.end() ||
      !(it->second->observers.might_have_observers())) {
    DVLOG_WITH_INSTANCE(1)
        << "Received route list without any active observers: " << media_source;
    return;
  }

  auto* routes_query = it->second.get();
  routes_query->cached_route_list = routes;
  routes_query->joinable_route_ids = joinable_route_ids;

  if (routes_query->observers.might_have_observers()) {
    for (auto& observer : routes_query->observers)
      observer.OnRoutesUpdated(routes, joinable_route_ids);
  } else {
    DVLOG_WITH_INSTANCE(1)
        << "Received routes update without any active observers: "
        << media_source;
  }
  RemoveInvalidRouteControllers(routes);
}

void MediaRouterMojoImpl::RouteResponseReceived(
    const std::string& presentation_id,
    bool is_incognito,
    std::vector<MediaRouteResponseCallback> callbacks,
    bool is_join,
    const base::Optional<MediaRoute>& media_route,
    const base::Optional<std::string>& error_text,
    RouteRequestResult::ResultCode result_code) {
  std::unique_ptr<RouteRequestResult> result;
  if (!media_route) {
    // An error occurred.
    const std::string& error = (error_text && !error_text->empty())
        ? *error_text : std::string("Unknown error.");
    result = RouteRequestResult::FromError(error, result_code);
  } else if (media_route->is_incognito() != is_incognito) {
    std::string error = base::StringPrintf(
        "Mismatch in incognito status: request = %d, response = %d",
        is_incognito, media_route->is_incognito());
    result = RouteRequestResult::FromError(
        error, RouteRequestResult::INCOGNITO_MISMATCH);
  } else {
    result =
        RouteRequestResult::FromSuccess(media_route.value(), presentation_id);
  }

  if (is_join)
    MediaRouterMojoMetrics::RecordJoinRouteResultCode(result->result_code());
  else
    MediaRouterMojoMetrics::RecordCreateRouteResultCode(result->result_code());

  RunRouteRequestCallbacks(std::move(result), std::move(callbacks));
}

void MediaRouterMojoImpl::CreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const url::Origin& origin,
    content::WebContents* web_contents,
    std::vector<MediaRouteResponseCallback> callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int tab_id = SessionTabHelper::IdForTab(web_contents);
  DoCreateRoute(source_id, sink_id, origin, tab_id, std::move(callbacks),
                timeout, incognito);
}

void MediaRouterMojoImpl::JoinRoute(
    const MediaSource::Id& source_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    content::WebContents* web_contents,
    std::vector<MediaRouteResponseCallback> callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!HasJoinableRoute()) {
    DVLOG_WITH_INSTANCE(1) << "No joinable routes";
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Route not found", RouteRequestResult::ROUTE_NOT_FOUND);
    MediaRouterMojoMetrics::RecordJoinRouteResultCode(result->result_code());
    RunRouteRequestCallbacks(std::move(result), std::move(callbacks));
    return;
  }

  int tab_id = SessionTabHelper::IdForTab(web_contents);
  DoJoinRoute(source_id, presentation_id, origin, tab_id, std::move(callbacks),
              timeout, incognito);
}

void MediaRouterMojoImpl::ConnectRouteByRouteId(
    const MediaSource::Id& source_id,
    const MediaRoute::Id& route_id,
    const url::Origin& origin,
    content::WebContents* web_contents,
    std::vector<MediaRouteResponseCallback> callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int tab_id = SessionTabHelper::IdForTab(web_contents);
  DoConnectRouteByRouteId(source_id, route_id, origin, tab_id,
                          std::move(callbacks), timeout, incognito);
}

void MediaRouterMojoImpl::TerminateRoute(const MediaRoute::Id& route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(2) << "TerminateRoute " << route_id;
  DoTerminateRoute(route_id);
}

void MediaRouterMojoImpl::DetachRoute(const MediaRoute::Id& route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoDetachRoute(route_id);
}

void MediaRouterMojoImpl::SendRouteMessage(const MediaRoute::Id& route_id,
                                           const std::string& message,
                                           SendRouteMessageCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoSendRouteMessage(route_id, message, std::move(callback));
}

void MediaRouterMojoImpl::SendRouteBinaryMessage(
    const MediaRoute::Id& route_id,
    std::unique_ptr<std::vector<uint8_t>> data,
    SendRouteMessageCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoSendRouteBinaryMessage(route_id, std::move(data), std::move(callback));
}

void MediaRouterMojoImpl::AddIssue(const IssueInfo& issue_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issue_manager_.AddIssue(issue_info);
}

void MediaRouterMojoImpl::ClearIssue(const Issue::Id& issue_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issue_manager_.ClearIssue(issue_id);
}

void MediaRouterMojoImpl::OnUserGesture() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (cast_media_sink_service_)
    cast_media_sink_service_->ForceDiscovery();
}

void MediaRouterMojoImpl::SearchSinks(
    const MediaSink::Id& sink_id,
    const MediaSource::Id& source_id,
    const std::string& search_input,
    const std::string& domain,
    MediaSinkSearchResponseCallback sink_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoSearchSinks(sink_id, source_id, search_input, domain,
                std::move(sink_callback));
}

scoped_refptr<MediaRouteController> MediaRouterMojoImpl::GetRouteController(
    const MediaRoute::Id& route_id) {
  auto* route = GetRoute(route_id);
  if (!route) {
    DVLOG_WITH_INSTANCE(1) << __func__ << ": route not found: " << route_id;
    return nullptr;
  }

  auto it = route_controllers_.find(route_id);
  if (it != route_controllers_.end())
    return scoped_refptr<MediaRouteController>(it->second);

  scoped_refptr<MediaRouteController> route_controller;
  switch (route->controller_type()) {
    case RouteControllerType::kNone:
      DVLOG_WITH_INSTANCE(1)
          << __func__ << ": route does not support controller: " << route_id;
      return nullptr;
    case RouteControllerType::kGeneric:
      route_controller = new MediaRouteController(route_id, context_);
      break;
    case RouteControllerType::kHangouts:
      route_controller = new HangoutsMediaRouteController(route_id, context_);
      break;
  }
  DCHECK(route_controller);
  route_controllers_.emplace(route_id, route_controller.get());

  DoCreateMediaRouteController(route_controller.get());
  return route_controller;
}

void MediaRouterMojoImpl::ProvideSinks(const std::string& provider_name,
                                       std::vector<MediaSinkInternal> sinks) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG_WITH_INSTANCE(1) << "Provider [" << provider_name << "] found "
                         << sinks.size() << " devices...";
  DoProvideSinks(provider_name, std::move(sinks));
}

bool MediaRouterMojoImpl::RegisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Create an observer list for the media source and add |observer|
  // to it. Fail if |observer| is already registered.
  const std::string& source_id = observer->source().id();
  std::unique_ptr<MediaSinksQuery>& sinks_query = sinks_queries_[source_id];
  bool is_new_query = false;
  if (!sinks_query) {
    is_new_query = true;
    sinks_query = base::MakeUnique<MediaSinksQuery>();
  } else {
    DCHECK(!sinks_query->observers.HasObserver(observer));
  }

  // If sink availability is UNAVAILABLE, then there is no need to call MRPM.
  // |observer| can be immediately notified with an empty list.
  sinks_query->observers.AddObserver(observer);
  if (availability_ == mojom::MediaRouter::SinkAvailability::UNAVAILABLE) {
    observer->OnSinksUpdated(std::vector<MediaSink>(),
                             std::vector<url::Origin>());
  } else {
    // Need to call MRPM to start observing sinks if the query is new.
    if (is_new_query) {
      DoStartObservingMediaSinks(source_id);
    } else if (sinks_query->cached_sink_list) {
      observer->OnSinksUpdated(*sinks_query->cached_sink_list,
                               sinks_query->origins);
    }
  }
  return true;
}

void MediaRouterMojoImpl::UnregisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const MediaSource::Id& source_id = observer->source().id();
  auto it = sinks_queries_.find(source_id);
  if (it == sinks_queries_.end() ||
      !it->second->observers.HasObserver(observer)) {
    return;
  }

  // If we are removing the final observer for the source, then stop
  // observing sinks for it.
  // might_have_observers() is reliable here on the assumption that this call
  // is not inside the ObserverList iteration.
  it->second->observers.RemoveObserver(observer);
  if (!it->second->observers.might_have_observers()) {
    // Only ask MRPM to stop observing media sinks if the availability is not
    // UNAVAILABLE.
    // Otherwise, the MRPM would have discarded the queries already.
    if (availability_ !=
        mojom::MediaRouter::SinkAvailability::UNAVAILABLE) {
      // The |sinks_queries_| entry will be removed in the immediate or deferred
      // |DoStopObservingMediaSinks| call.
      DoStopObservingMediaSinks(source_id);
    } else {
      sinks_queries_.erase(source_id);
    }
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
    routes_query = base::MakeUnique<MediaRoutesQuery>();
  } else {
    DCHECK(!routes_query->observers.HasObserver(observer));
  }

  routes_query->observers.AddObserver(observer);
  if (is_new_query) {
    DoStartObservingMediaRoutes(source_id);
    // The MRPM will call MediaRouterMojoImpl::OnRoutesUpdated() soon, if there
    // are any existing routes the new observer should be aware of.
  } else if (routes_query->cached_route_list) {
    // Return to the event loop before notifying of a cached route list because
    // MediaRoutesObserver is calling this method from its constructor, and that
    // must complete before invoking its virtual OnRoutesUpdated() method.
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
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
  if (it == routes_queries_.end() || !it->second->cached_route_list ||
      !it->second->observers.HasObserver(observer)) {
    return;
  }

  observer->OnRoutesUpdated(*it->second->cached_route_list,
                            it->second->joinable_route_ids);
}

void MediaRouterMojoImpl::UnregisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  const MediaSource::Id source_id = observer->source_id();
  auto it = routes_queries_.find(source_id);
  if (it == routes_queries_.end() ||
      !it->second->observers.HasObserver(observer)) {
    return;
  }

  // If we are removing the final observer for the source, then stop
  // observing routes for it.
  // might_have_observers() is reliable here on the assumption that this call
  // is not inside the ObserverList iteration.
  it->second->observers.RemoveObserver(observer);
  if (!it->second->observers.might_have_observers())
    DoStopObservingMediaRoutes(source_id);
}

void MediaRouterMojoImpl::RegisterIssuesObserver(IssuesObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issue_manager_.RegisterObserver(observer);
}

void MediaRouterMojoImpl::UnregisterIssuesObserver(IssuesObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issue_manager_.UnregisterObserver(observer);
}

void MediaRouterMojoImpl::RegisterRouteMessageObserver(
    RouteMessageObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  const MediaRoute::Id& route_id = observer->route_id();
  auto& observer_list = message_observers_[route_id];
  if (!observer_list) {
    observer_list =
        base::MakeUnique<base::ObserverList<RouteMessageObserver>>();
  } else {
    DCHECK(!observer_list->HasObserver(observer));
  }

  bool should_listen = !observer_list->might_have_observers();
  observer_list->AddObserver(observer);
  if (should_listen)
    DoStartListeningForRouteMessages(route_id);
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
    DoStopListeningForRouteMessages(route_id);
  }
}

void MediaRouterMojoImpl::DetachRouteController(
    const MediaRoute::Id& route_id,
    MediaRouteController* controller) {
  auto it = route_controllers_.find(route_id);
  if (it != route_controllers_.end() && it->second == controller)
    route_controllers_.erase(it);
}

void MediaRouterMojoImpl::DoCreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const url::Origin& origin,
    int tab_id,
    std::vector<MediaRouteResponseCallback> callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  std::string presentation_id = MediaRouterBase::CreatePresentationId();
  DVLOG_WITH_INSTANCE(1) << "DoCreateRoute " << source_id << "=>" << sink_id
                         << ", presentation ID: " << presentation_id;
  media_route_provider_->CreateRoute(
      source_id, sink_id, presentation_id, origin, tab_id, timeout, incognito,
      base::BindOnce(&MediaRouterMojoImpl::RouteResponseReceived,
                     base::Unretained(this), presentation_id, incognito,
                     base::Passed(&callbacks), false));
}

void MediaRouterMojoImpl::DoJoinRoute(
    const MediaSource::Id& source_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int tab_id,
    std::vector<MediaRouteResponseCallback> callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  DVLOG_WITH_INSTANCE(1) << "DoJoinRoute " << source_id
                         << ", presentation ID: " << presentation_id;

  media_route_provider_->JoinRoute(
      source_id, presentation_id, origin, tab_id, timeout, incognito,
      base::BindOnce(&MediaRouterMojoImpl::RouteResponseReceived,
                     base::Unretained(this), presentation_id, incognito,
                     base::Passed(&callbacks), true));
}

void MediaRouterMojoImpl::DoConnectRouteByRouteId(
    const MediaSource::Id& source_id,
    const MediaRoute::Id& route_id,
    const url::Origin& origin,
    int tab_id,
    std::vector<MediaRouteResponseCallback> callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  std::string presentation_id = MediaRouterBase::CreatePresentationId();
  DVLOG_WITH_INSTANCE(1) << "DoConnectRouteByRouteId " << source_id
                         << ", route ID: " << route_id
                         << ", presentation ID: " << presentation_id;

  media_route_provider_->ConnectRouteByRouteId(
      source_id, route_id, presentation_id, origin, tab_id, timeout, incognito,
      base::BindOnce(&MediaRouterMojoImpl::RouteResponseReceived,
                     base::Unretained(this), presentation_id, incognito,
                     base::Passed(&callbacks), true));
}

void MediaRouterMojoImpl::DoTerminateRoute(const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "DoTerminateRoute " << route_id;
  media_route_provider_->TerminateRoute(
      route_id, base::BindOnce(&MediaRouterMojoImpl::OnTerminateRouteResult,
                               base::Unretained(this), route_id));
}

void MediaRouterMojoImpl::DoDetachRoute(const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "DoDetachRoute " << route_id;
  media_route_provider_->DetachRoute(route_id);
}

void MediaRouterMojoImpl::DoSendRouteMessage(
    const MediaRoute::Id& route_id,
    const std::string& message,
    SendRouteMessageCallback callback) {
  DVLOG_WITH_INSTANCE(1) << "SendRouteMessage " << route_id;
  media_route_provider_->SendRouteMessage(route_id, message,
                                          std::move(callback));
}

void MediaRouterMojoImpl::DoSendRouteBinaryMessage(
    const MediaRoute::Id& route_id,
    std::unique_ptr<std::vector<uint8_t>> data,
    SendRouteMessageCallback callback) {
  DVLOG_WITH_INSTANCE(1) << "SendRouteBinaryMessage " << route_id;
  media_route_provider_->SendRouteBinaryMessage(route_id, *data,
                                                std::move(callback));
}

void MediaRouterMojoImpl::DoStartListeningForRouteMessages(
    const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStartListeningForRouteMessages";
  media_route_provider_->StartListeningForRouteMessages(route_id);
}

void MediaRouterMojoImpl::DoStopListeningForRouteMessages(
    const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "StopListeningForRouteMessages";
  media_route_provider_->StopListeningForRouteMessages(route_id);
}

void MediaRouterMojoImpl::DoSearchSinks(
    const MediaSink::Id& sink_id,
    const MediaSource::Id& source_id,
    const std::string& search_input,
    const std::string& domain,
    MediaSinkSearchResponseCallback sink_callback) {
  DVLOG_WITH_INSTANCE(1) << "SearchSinks";
  auto sink_search_criteria = mojom::SinkSearchCriteria::New();
  sink_search_criteria->input = search_input;
  sink_search_criteria->domain = domain;
  media_route_provider_->SearchSinks(sink_id, source_id,
                                     std::move(sink_search_criteria),
                                     std::move(sink_callback));
}

void MediaRouterMojoImpl::DoProvideSinks(const std::string& provider_name,
                                         std::vector<MediaSinkInternal> sinks) {
  DVLOG_WITH_INSTANCE(1) << "DoProvideSinks";
  media_route_provider_->ProvideSinks(provider_name, sinks);
}

void MediaRouterMojoImpl::DoCreateMediaRouteController(
    MediaRouteController* controller) {
  DVLOG_WITH_INSTANCE(1) << "DoCreateMediaRouteController";
  auto controller_request = controller->CreateControllerRequest();
  auto observer_ptr = controller->BindObserverPtr();
  const MediaRoute::Id& route_id = controller->route_id();
  if (!controller_request.is_pending() || !observer_ptr.is_bound()) {
    DVLOG_WITH_INSTANCE(1) << __func__
                           << ": invalid Mojo request/ptr: " << route_id;
    return;
  }

  media_route_provider_->CreateMediaRouteController(
      route_id, std::move(controller_request), std::move(observer_ptr),
      base::BindOnce(&MediaRouterMojoImpl::OnMediaControllerCreated,
                     base::Unretained(this), route_id));
  controller->InitAdditionalMojoConnnections();
}

void MediaRouterMojoImpl::OnRouteMessagesReceived(
    const std::string& route_id,
    const std::vector<content::PresentationConnectionMessage>& messages) {
  DVLOG_WITH_INSTANCE(1) << "OnRouteMessagesReceived";

  if (messages.empty())
    return;

  auto it = message_observers_.find(route_id);
  if (it == message_observers_.end())
    return;

  for (auto& observer : *it->second)
    observer.OnMessagesReceived(messages);
}

void MediaRouterMojoImpl::OnSinkAvailabilityUpdated(
    SinkAvailability availability) {
  if (availability_ == availability)
    return;

  availability_ = availability;
  if (availability_ == mojom::MediaRouter::SinkAvailability::UNAVAILABLE) {
    // Sinks are no longer available. MRPM has already removed all sink queries.
    for (auto& source_and_query : sinks_queries_) {
      const auto& query = source_and_query.second;
      query->is_active = false;
      query->cached_sink_list = base::nullopt;
      query->origins.clear();
    }
  } else {
    // Sinks are now available. Tell MRPM to start all sink queries again.
    for (const auto& source_and_query : sinks_queries_)
      DoStartObservingMediaSinks(source_and_query.first);
  }
}

void MediaRouterMojoImpl::OnPresentationConnectionStateChanged(
    const std::string& route_id,
    content::PresentationConnectionState state) {
  NotifyPresentationConnectionStateChange(route_id, state);
}

void MediaRouterMojoImpl::OnPresentationConnectionClosed(
    const std::string& route_id,
    content::PresentationConnectionCloseReason reason,
    const std::string& message) {
  NotifyPresentationConnectionClose(route_id, reason, message);
}

void MediaRouterMojoImpl::OnTerminateRouteResult(
    const MediaRoute::Id& route_id,
    const base::Optional<std::string>& error_text,
    RouteRequestResult::ResultCode result_code) {
  if (result_code != RouteRequestResult::OK) {
    LOG(WARNING) << "Failed to terminate route " << route_id
                 << ": result_code = " << result_code << ", "
                 << error_text.value_or(std::string());
  }
  MediaRouterMojoMetrics::RecordMediaRouteProviderTerminateRoute(result_code);
}

void MediaRouterMojoImpl::DoStartObservingMediaSinks(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStartObservingMediaSinks: " << source_id;
  // No need to call MRPM if there are no sinks available.
  if (availability_ == mojom::MediaRouter::SinkAvailability::UNAVAILABLE)
    return;

  // No need to call MRPM if all observers have been removed in the meantime.
  auto it = sinks_queries_.find(source_id);
  if (it == sinks_queries_.end() ||
      !it->second->observers.might_have_observers())
    return;

  DVLOG_WITH_INSTANCE(1) << "MRPM.StartObservingMediaSinks: " << source_id;
  media_route_provider_->StartObservingMediaSinks(source_id);
  it->second->is_active = true;
}

void MediaRouterMojoImpl::DoStopObservingMediaSinks(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStopObservingMediaSinks: " << source_id;

  auto it = sinks_queries_.find(source_id);
  // No need to call MRPM if observers have been added in the meantime,
  // or StopObservingMediaSinks has already been called.
  if (it == sinks_queries_.end() || !it->second->is_active ||
      it->second->observers.might_have_observers()) {
    return;
  }

  DVLOG_WITH_INSTANCE(1) << "MRPM.StopObservingMediaSinks: " << source_id;
  media_route_provider_->StopObservingMediaSinks(source_id);
  sinks_queries_.erase(source_id);
}

void MediaRouterMojoImpl::DoStartObservingMediaRoutes(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStartObservingMediaRoutes";

  // No need to call MRPM if all observers have been removed in the meantime.
  auto it = routes_queries_.find(source_id);
  if (it == routes_queries_.end() ||
      !it->second->observers.might_have_observers())
    return;

  DVLOG_WITH_INSTANCE(1) << "MRPM.StartObservingMediaRoutes: " << source_id;
  media_route_provider_->StartObservingMediaRoutes(source_id);
}

void MediaRouterMojoImpl::DoStopObservingMediaRoutes(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStopObservingMediaRoutes";

  // No need to call MRPM if observers have been added in the meantime,
  // or StopObservingMediaRoutes has already been called.
  auto it = routes_queries_.find(source_id);
  if (it == routes_queries_.end() ||
      it->second->observers.might_have_observers()) {
    return;
  }

  DVLOG_WITH_INSTANCE(1) << "MRPM.StopObservingMediaRoutes: " << source_id;
  media_route_provider_->StopObservingMediaRoutes(source_id);
  routes_queries_.erase(source_id);
}

void MediaRouterMojoImpl::SyncStateToMediaRouteProvider() {
  DCHECK(media_route_provider_);

  // Sink queries.
  if (availability_ != mojom::MediaRouter::SinkAvailability::UNAVAILABLE) {
    for (const auto& it : sinks_queries_)
      DoStartObservingMediaSinks(it.first);
  }

  // Route queries.
  for (const auto& it : routes_queries_)
    DoStartObservingMediaRoutes(it.first);

  // Route messages.
  for (const auto& it : message_observers_)
    DoStartListeningForRouteMessages(it.first);

  StartDiscovery();
}

void MediaRouterMojoImpl::StartDiscovery() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG_WITH_INSTANCE(1) << "StartDiscovery";

  if (media_router::CastDiscoveryEnabled()) {
    if (!cast_media_sink_service_) {
      cast_media_sink_service_ = new CastMediaSinkService(
          base::Bind(&MediaRouterMojoImpl::ProvideSinks,
                     weak_factory_.GetWeakPtr(), "cast"),
          context_,
          content::BrowserThread::GetTaskRunnerForThread(
              content::BrowserThread::IO));
    }
    cast_media_sink_service_->Start();
  }

  if (media_router::DialLocalDiscoveryEnabled()) {
    if (!dial_media_sink_service_proxy_) {
      dial_media_sink_service_proxy_ = new DialMediaSinkServiceProxy(
          base::Bind(&MediaRouterMojoImpl::ProvideSinks,
                     weak_factory_.GetWeakPtr(), "dial"),
          context_);
      dial_media_sink_service_proxy_->SetObserver(
          cast_media_sink_service_.get());
    }
    dial_media_sink_service_proxy_->Start();
  }
}

void MediaRouterMojoImpl::UpdateMediaSinks(
    const MediaSource::Id& source_id) {
  DoUpdateMediaSinks(source_id);
}

void MediaRouterMojoImpl::DoUpdateMediaSinks(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoUpdateMediaSinks: " << source_id;
  media_route_provider_->UpdateMediaSinks(source_id);
}

void MediaRouterMojoImpl::RemoveInvalidRouteControllers(
    const std::vector<MediaRoute>& routes) {
  auto it = route_controllers_.begin();
  while (it != route_controllers_.end()) {
    if (GetRoute(it->first)) {
      ++it;
    } else {
      it->second->Invalidate();
      it = route_controllers_.erase(it);
    }
  }
}

void MediaRouterMojoImpl::OnMediaControllerCreated(
    const MediaRoute::Id& route_id,
    bool success) {
  DVLOG_WITH_INSTANCE(1) << "OnMediaControllerCreated: " << route_id
                         << (success ? " was successful." : " failed.");
  MediaRouterMojoMetrics::RecordMediaRouteControllerCreationResult(success);
}

void MediaRouterMojoImpl::OnMediaRemoterCreated(
    int32_t tab_id,
    media::mojom::MirrorServiceRemoterPtr remoter,
    media::mojom::MirrorServiceRemotingSourceRequest source_request) {
  DVLOG_WITH_INSTANCE(1) << __func__ << ": tab_id = " << tab_id;

  auto it = remoting_sources_.find(tab_id);
  if (it == remoting_sources_.end()) {
    LOG(WARNING) << __func__
                 << ": No registered remoting source for tab_id = " << tab_id;
    return;
  }

  CastRemotingConnector* connector = it->second;
  connector->ConnectToService(std::move(source_request), std::move(remoter));
}

}  // namespace media_router
