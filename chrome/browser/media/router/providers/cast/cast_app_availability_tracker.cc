// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_app_availability_tracker.h"

namespace media_router {

CastAppAvailabilityTracker::CastAppAvailabilityTracker() {}
CastAppAvailabilityTracker::~CastAppAvailabilityTracker() = default;

base::flat_set<std::string> CastAppAvailabilityTracker::RegisterSource(
    const CastMediaSource& source) {
  if (base::ContainsKey(registered_sources_, source.source_id()))
    return base::flat_set<std::string>();

  registered_sources_.emplace(source.source_id(), source);

  base::flat_set<std::string> new_app_ids;
  for (const auto& app_info : source.app_infos()) {
    const auto& app_id = app_info.app_id;
    if (++registration_count_by_app_id_[app_id] == 1)
      new_app_ids.insert(app_id);
  }
  return new_app_ids;
}

void CastAppAvailabilityTracker::UnregisterSource(
    const CastMediaSource& source) {
  auto it = registered_sources_.find(source.source_id());
  if (it == registered_sources_.end())
    return;

  for (const auto& app_info : it->second.app_infos()) {
    const std::string& app_id = app_info.app_id;
    auto count_it = registration_count_by_app_id_.find(app_id);
    DCHECK(count_it != registration_count_by_app_id_.end());
    if (--(count_it->second) == 0)
      registration_count_by_app_id_.erase(count_it);
  }

  registered_sources_.erase(it);
}

std::vector<CastMediaSource> CastAppAvailabilityTracker::UpdateAppAvailability(
    const MediaSink::Id& sink_id,
    const std::string& app_id,
    AppAvailability availability) {
  auto& availabilities = app_availabilities_[sink_id];
  auto it = availabilities.find(app_id);

  // Updated if value changes from unknown / unavailable to available, or from
  // available to unavailble.
  AppAvailability old_availability =
      it != availabilities.end() ? it->second : AppAvailability::kUnavailable;
  bool updated = old_availability != availability;
  availabilities[app_id] = availability;

  if (!updated)
    return std::vector<CastMediaSource>();

  std::vector<CastMediaSource> affected_sources;
  for (const auto& source : registered_sources_) {
    if (source.second.ContainsApp(app_id))
      affected_sources.push_back(source.second);
  }
  return affected_sources;
}

std::vector<CastMediaSource> CastAppAvailabilityTracker::RemoveResultsForSink(
    const MediaSink::Id& sink_id) {
  auto it = app_availabilities_.find(sink_id);
  if (it == app_availabilities_.end())
    return std::vector<CastMediaSource>();

  // Find all app IDs that were available on the sink.
  std::vector<std::string> affected_app_ids;
  for (const auto& availability : it->second) {
    if (availability.second == AppAvailability::kAvailable)
      affected_app_ids.push_back(availability.first);
  }

  // Find all registered sources whose query results need to be updated.
  std::vector<CastMediaSource> affected_sources;
  for (const auto& source : registered_sources_) {
    if (source.second.ContainsAnyAppFrom(affected_app_ids))
      affected_sources.push_back(source.second);
  }

  app_availabilities_.erase(it);
  return affected_sources;
}

bool CastAppAvailabilityTracker::IsAvailabilityKnown(
    const MediaSink::Id& sink_id,
    const std::string& app_id) const {
  auto availabilities_it = app_availabilities_.find(sink_id);
  return availabilities_it != app_availabilities_.end() &&
         base::ContainsKey(availabilities_it->second, app_id);
}

std::vector<std::string> CastAppAvailabilityTracker::GetRegisteredApps() const {
  std::vector<std::string> registered_apps;
  for (const auto& app_ids_and_count : registration_count_by_app_id_)
    registered_apps.push_back(app_ids_and_count.first);

  return registered_apps;
}

base::flat_set<MediaSink::Id> CastAppAvailabilityTracker::GetAvailableSinks(
    const CastMediaSource& source) const {
  base::flat_set<MediaSink::Id> sink_ids;
  // For each sink, check if there is at least one available app in |source|.
  for (const auto& availabilities : app_availabilities_) {
    for (const auto& app_info : source.app_infos()) {
      const auto& availabilities_map = availabilities.second;
      auto availability_it = availabilities_map.find(app_info.app_id);
      if (availability_it != availabilities_map.end() &&
          availability_it->second == AppAvailability::kAvailable) {
        sink_ids.insert(availabilities.first);
        break;
      }
    }
  }
  return sink_ids;
}

}  // namespace media_router
