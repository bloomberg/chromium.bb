// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_media_route_provider.h"

#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "chrome/common/media_router/media_source_helper.h"
#include "url/origin.h"

namespace media_router {

namespace {

url::Origin CreateOrigin(const std::string& url) {
  return url::Origin::Create(GURL(url));
}

}  // namespace

DialMediaRouteProvider::DialMediaRouteProvider(
    mojom::MediaRouteProviderRequest request,
    mojom::MediaRouterPtr media_router,
    DialMediaSinkService* dial_media_sink_service)
    : binding_(this, std::move(request)),
      media_router_(std::move(media_router)),
      dial_media_sink_service_(dial_media_sink_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(dial_media_sink_service_);
  // TODO(crbug.com/808720): This needs to be set properly according to sinks
  // discovered.
  media_router_->OnSinkAvailabilityUpdated(
      MediaRouteProviderId::DIAL,
      mojom::MediaRouter::SinkAvailability::PER_SOURCE);
}

DialMediaRouteProvider::~DialMediaRouteProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_sink_queries_.empty());
}

void DialMediaRouteProvider::CreateRoute(const std::string& media_source,
                                         const std::string& sink_id,
                                         const std::string& presentation_id,
                                         const url::Origin& origin,
                                         int32_t tab_id,
                                         base::TimeDelta timeout,
                                         bool incognito,
                                         CreateRouteCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(
      base::nullopt, std::string("Not implemented"),
      RouteRequestResult::ResultCode::NO_SUPPORTED_PROVIDER);
}

void DialMediaRouteProvider::JoinRoute(const std::string& media_source,
                                       const std::string& presentation_id,
                                       const url::Origin& origin,
                                       int32_t tab_id,
                                       base::TimeDelta timeout,
                                       bool incognito,
                                       JoinRouteCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(
      base::nullopt, std::string("Not implemented"),
      RouteRequestResult::ResultCode::NO_SUPPORTED_PROVIDER);
}

void DialMediaRouteProvider::ConnectRouteByRouteId(
    const std::string& media_source,
    const std::string& route_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    ConnectRouteByRouteIdCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(
      base::nullopt, std::string("Not implemented"),
      RouteRequestResult::ResultCode::NO_SUPPORTED_PROVIDER);
}

void DialMediaRouteProvider::TerminateRoute(const std::string& route_id,
                                            TerminateRouteCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(
      std::string("Not implemented"),
      RouteRequestResult::ResultCode::NO_SUPPORTED_PROVIDER);
}

void DialMediaRouteProvider::SendRouteMessage(
    const std::string& media_route_id,
    const std::string& message,
    SendRouteMessageCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}

void DialMediaRouteProvider::SendRouteBinaryMessage(
    const std::string& media_route_id,
    const std::vector<uint8_t>& data,
    SendRouteBinaryMessageCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}

void DialMediaRouteProvider::StartObservingMediaSinks(
    const std::string& media_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(dial_media_sink_service_);

  MediaSource dial_source(media_source);
  if (!IsDialMediaSource(dial_source))
    return;

  RegisterDialMediaSource(dial_source);
}

void DialMediaRouteProvider::StopObservingMediaSinks(
    const std::string& media_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(dial_media_sink_service_);

  MediaSource dial_source(media_source);
  if (!IsDialMediaSource(dial_source))
    return;

  UnregisterDialMediaSource(dial_source);
}

void DialMediaRouteProvider::StartObservingMediaRoutes(
    const std::string& media_source) {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::StopObservingMediaRoutes(
    const std::string& media_source) {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::StartListeningForRouteMessages(
    const std::string& route_id) {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::StopListeningForRouteMessages(
    const std::string& route_id) {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::DetachRoute(const std::string& route_id) {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::EnableMdnsDiscovery() {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::UpdateMediaSinks(const std::string& media_source) {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::SearchSinks(
    const std::string& sink_id,
    const std::string& media_source,
    mojom::SinkSearchCriteriaPtr search_criteria,
    SearchSinksCallback callback) {
  std::move(callback).Run(std::string());
}

void DialMediaRouteProvider::ProvideSinks(
    const std::string& provider_name,
    const std::vector<media_router::MediaSinkInternal>& sinks) {
  NOTIMPLEMENTED();
}

void DialMediaRouteProvider::CreateMediaRouteController(
    const std::string& route_id,
    mojom::MediaControllerRequest media_controller,
    mojom::MediaStatusObserverPtr observer,
    CreateMediaRouteControllerCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}

void DialMediaRouteProvider::OnAvailableSinksUpdated(
    const std::string& app_name,
    const std::vector<MediaSinkInternal>& sinks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto& sink_query_it = media_sink_queries_.find(app_name);
  if (sink_query_it == media_sink_queries_.end()) {
    DVLOG(2) << "Not monitoring app " << app_name;
    return;
  }

  const auto& media_sources = sink_query_it->second->media_sources;
  std::vector<url::Origin> origins = GetOrigins(app_name);

  for (const auto& media_source : media_sources) {
    media_router_->OnSinksReceived(MediaRouteProviderId::DIAL,
                                   media_source.id(), sinks, origins);
  }
}

void DialMediaRouteProvider::RegisterDialMediaSource(
    const MediaSource& dial_source) {
  std::string app_name = AppNameFromDialMediaSource(dial_source);
  auto& sink_query = media_sink_queries_[app_name];
  if (!sink_query) {
    sink_query = std::make_unique<MediaSinkQuery>();
    sink_query->subscription =
        dial_media_sink_service_->StartMonitoringAvailableSinksForApp(
            app_name, base::BindRepeating(
                          &DialMediaRouteProvider::OnAvailableSinksUpdated,
                          base::Unretained(this)));
  }

  if (sink_query->media_sources.insert(dial_source).second)
    MayNotifyMediaSinksObservers(dial_source.id(), app_name);
}

void DialMediaRouteProvider::MayNotifyMediaSinksObservers(
    const MediaSource::Id& media_source_id,
    const std::string& app_name) {
  const auto& cached_sinks =
      dial_media_sink_service_->GetCachedAvailableSinks(app_name);
  if (cached_sinks.empty())
    return;

  media_router_->OnSinksReceived(MediaRouteProviderId::DIAL, media_source_id,
                                 cached_sinks, GetOrigins(app_name));
}

void DialMediaRouteProvider::UnregisterDialMediaSource(
    const MediaSource& dial_source) {
  std::string app_name = AppNameFromDialMediaSource(dial_source);
  const auto& sink_query_it = media_sink_queries_.find(app_name);
  if (sink_query_it == media_sink_queries_.end())
    return;

  auto& media_sources = sink_query_it->second->media_sources;
  media_sources.erase(dial_source);
  if (!media_sources.empty())
    return;

  media_sink_queries_.erase(app_name);
}

std::vector<url::Origin> DialMediaRouteProvider::GetOrigins(
    const std::string& app_name) {
  static const base::NoDestructor<
      base::flat_map<std::string, std::vector<url::Origin>>>
      origin_white_list(
          {{"YouTube",
            {CreateOrigin("https://tv.youtube.com"),
             CreateOrigin("https://tv-green-qa.youtube.com"),
             CreateOrigin("https://tv-release-qa.youtube.com"),
             CreateOrigin("https://web-green-qa.youtube.com"),
             CreateOrigin("https://web-release-qa.youtube.com"),
             CreateOrigin("https://www.youtube.com")}},
           {"Netflix", {CreateOrigin("https://www.netflix.com")}},
           {"Pandora", {CreateOrigin("https://www.pandora.com")}},
           {"Radio", {CreateOrigin("https://www.pandora.com")}},
           {"Hulu", {CreateOrigin("https://www.hulu.com")}},
           {"Vimeo", {CreateOrigin("https://www.vimeo.com")}},
           {"Dailymotion", {CreateOrigin("https://www.dailymotion.com")}},
           {"com.dailymotion", {CreateOrigin("https://www.dailymotion.com")}}});

  auto origins_it = origin_white_list->find(app_name);
  if (origins_it == origin_white_list->end())
    return std::vector<url::Origin>();

  return origins_it->second;
}

DialMediaRouteProvider::MediaSinkQuery::MediaSinkQuery() = default;
DialMediaRouteProvider::MediaSinkQuery::~MediaSinkQuery() = default;

}  // namespace media_router
