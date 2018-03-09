// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_app_discovery_service.h"

#include "chrome/browser/media/router/providers/cast/cast_media_route_provider_metrics.h"
#include "components/cast_channel/cast_message_handler.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"

namespace media_router {

CastAppDiscoveryService::CastAppDiscoveryService(
    cast_channel::CastMessageHandler* message_handler,
    cast_channel::CastSocketService* socket_service)
    : message_handler_(message_handler),
      socket_service_(socket_service),
      weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CastAppDiscoveryService::~CastAppDiscoveryService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CastAppDiscoveryService::Subscription
CastAppDiscoveryService::StartObservingMediaSinks(
    const CastMediaSource& source,
    const SinkQueryCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const MediaSource::Id& source_id = source.source_id();

  // Returned cached results immediately, if available.
  base::flat_set<MediaSink::Id> cached_sink_ids =
      availability_tracker_.GetAvailableSinks(source);
  if (!cached_sink_ids.empty())
    callback.Run(source_id, GetSinksByIds(cached_sink_ids));

  auto& callback_list = sink_queries_[source_id];
  if (!callback_list) {
    callback_list = std::make_unique<SinkQueryCallbackList>();
    callback_list->set_removal_callback(
        base::BindRepeating(&CastAppDiscoveryService::MaybeRemoveSinkQueryEntry,
                            base::Unretained(this), source));

    // Note: even though we retain availability results for an app unregistered
    // from the tracker, we will send app availability requests again when it
    // is re-registered. This gives us a chance to refresh the results in case
    // it changed.
    base::flat_set<std::string> new_app_ids =
        availability_tracker_.RegisterSource(source);
    for (const auto& app_id : new_app_ids) {
      // Note: The following logic assumes |sinks_| will not change as it is
      // being iterated.
      for (const auto& sink : sinks_) {
        int channel_id = sink.second.cast_data().cast_channel_id;
        cast_channel::CastSocket* socket =
            socket_service_->GetSocket(channel_id);
        if (!socket) {
          DVLOG(1) << "Socket not found for id " << channel_id;
          continue;
        }

        RequestAppAvailability(socket, app_id, sink.first);
      }
    }
  }

  return callback_list->Add(callback);
}

void CastAppDiscoveryService::MaybeRemoveSinkQueryEntry(
    const CastMediaSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = sink_queries_.find(source.source_id());
  CHECK(it != sink_queries_.end());

  if (it->second->empty()) {
    availability_tracker_.UnregisterSource(source);
    sink_queries_.erase(it);
  }
}

void CastAppDiscoveryService::OnSinkAddedOrUpdated(
    const MediaSinkInternal& sink,
    cast_channel::CastSocket* socket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const MediaSink::Id& sink_id = sink.sink().id();
  sinks_.insert_or_assign(sink_id, sink);
  for (const std::string& app_id : availability_tracker_.GetRegisteredApps()) {
    if (availability_tracker_.IsAvailabilityKnown(sink_id, app_id))
      continue;

    RequestAppAvailability(socket, app_id, sink_id);
  }
}

void CastAppDiscoveryService::OnSinkRemoved(const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const MediaSink::Id& sink_id = sink.sink().id();
  sinks_.erase(sink_id);
  UpdateSinkQueries(availability_tracker_.RemoveResultsForSink(sink_id));
}

void CastAppDiscoveryService::RequestAppAvailability(
    cast_channel::CastSocket* socket,
    const std::string& app_id,
    const MediaSink::Id& sink_id) {
  message_handler_->RequestAppAvailability(
      socket, app_id,
      base::BindOnce(&CastAppDiscoveryService::UpdateAppAvailability,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     sink_id));
}

void CastAppDiscoveryService::UpdateAppAvailability(
    base::TimeTicks start_time,
    const MediaSink::Id& sink_id,
    const std::string& app_id,
    cast_channel::GetAppAvailabilityResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordAppAvailabilityResult(result, base::TimeTicks::Now() - start_time);
  if (!base::ContainsKey(sinks_, sink_id))
    return;

  if (result == cast_channel::GetAppAvailabilityResult::kUnknown) {
    // TODO(crbug.com/779892): Implement retry on user gesture.
    DVLOG(2) << "App availability unknown for sink: " << sink_id
             << ", app: " << app_id;
    return;
  }

  AppAvailability availability =
      result == cast_channel::GetAppAvailabilityResult::kAvailable
          ? AppAvailability::kAvailable
          : AppAvailability::kUnavailable;

  DVLOG(1) << "App " << app_id << " on sink " << sink_id << " is "
           << (availability == AppAvailability::kAvailable);
  UpdateSinkQueries(availability_tracker_.UpdateAppAvailability(sink_id, app_id,
                                                                availability));
}

void CastAppDiscoveryService::UpdateSinkQueries(
    const std::vector<CastMediaSource>& sources) {
  for (const auto& source : sources) {
    const MediaSource::Id& source_id = source.source_id();
    auto it = sink_queries_.find(source_id);
    if (it == sink_queries_.end())
      continue;
    base::flat_set<MediaSink::Id> sink_ids =
        availability_tracker_.GetAvailableSinks(source);
    it->second->Notify(source_id, GetSinksByIds(sink_ids));
  }
}

std::vector<MediaSinkInternal> CastAppDiscoveryService::GetSinksByIds(
    const base::flat_set<MediaSink::Id>& sink_ids) const {
  std::vector<MediaSinkInternal> sinks;
  for (const auto& sink_id : sink_ids) {
    auto it = sinks_.find(sink_id);
    if (it != sinks_.end())
      sinks.push_back(it->second);
  }
  return sinks;
}

}  // namespace media_router
