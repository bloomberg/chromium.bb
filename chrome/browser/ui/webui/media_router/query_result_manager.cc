// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/query_result_manager.h"

#include "base/containers/hash_tables.h"
#include "base/stl_util.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_sinks_observer.h"

namespace media_router {

// MediaSinkObserver that propagates results back to |result_manager|.
// An instance of this class is associated with each registered MediaCastMode.
class QueryResultManager::CastModeMediaSinksObserver
    : public MediaSinksObserver {
 public:
  CastModeMediaSinksObserver(MediaCastMode cast_mode,
                             const MediaSource& source,
                             MediaRouter* router,
                             QueryResultManager* result_manager)
      : MediaSinksObserver(router, source),
        cast_mode_(cast_mode),
        result_manager_(result_manager) {
    DCHECK(result_manager);
  }

  ~CastModeMediaSinksObserver() override {}

  // MediaSinksObserver
  void OnSinksReceived(const std::vector<MediaSink>& result) override {
    latest_sink_ids_.clear();
    for (const MediaSink& sink : result) {
      latest_sink_ids_.push_back(sink.id());
    }
    result_manager_->UpdateWithSinksQueryResult(cast_mode_, result);
    result_manager_->NotifyOnResultsUpdated();
  }

  // Returns the most recent sink IDs that were passed to |OnSinksReceived|.
  void GetLatestSinkIds(std::vector<MediaSink::Id>* sink_ids) const {
    DCHECK(sink_ids);
    *sink_ids = latest_sink_ids_;
  }

  MediaCastMode cast_mode() const { return cast_mode_; }

 private:
  MediaCastMode cast_mode_;
  std::vector<MediaSink::Id> latest_sink_ids_;
  QueryResultManager* result_manager_;
};

QueryResultManager::QueryResultManager(MediaRouter* router) : router_(router) {
  DCHECK(router_);
}

QueryResultManager::~QueryResultManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void QueryResultManager::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void QueryResultManager::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void QueryResultManager::StartSinksQuery(MediaCastMode cast_mode,
                                         const MediaSource& source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (source.Empty()) {
    LOG(WARNING) << "StartSinksQuery called with empty source for "
                 << cast_mode;
    return;
  }

  SetSourceForCastMode(cast_mode, source);
  RemoveObserverForCastMode(cast_mode);
  UpdateWithSinksQueryResult(cast_mode, std::vector<MediaSink>());

  linked_ptr<CastModeMediaSinksObserver> observer(
      new CastModeMediaSinksObserver(cast_mode, source, router_, this));
  observer->Init();
  auto result = sinks_observers_.insert(std::make_pair(cast_mode, observer));
  DCHECK(result.second);
  NotifyOnResultsUpdated();
}

void QueryResultManager::StopSinksQuery(MediaCastMode cast_mode) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RemoveObserverForCastMode(cast_mode);
  SetSourceForCastMode(cast_mode, MediaSource());
  UpdateWithSinksQueryResult(cast_mode, std::vector<MediaSink>());
  NotifyOnResultsUpdated();
}

void QueryResultManager::SetSourceForCastMode(
      MediaCastMode cast_mode, const MediaSource& source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  cast_mode_sources_[cast_mode] = source;
}

void QueryResultManager::RemoveObserverForCastMode(MediaCastMode cast_mode) {
  auto observers_it = sinks_observers_.find(cast_mode);
  if (observers_it != sinks_observers_.end())
    sinks_observers_.erase(observers_it);
}

bool QueryResultManager::IsValid(const MediaSinkWithCastModes& entry) const {
  return !entry.cast_modes.empty();
}

void QueryResultManager::UpdateWithSinksQueryResult(
    MediaCastMode cast_mode,
    const std::vector<MediaSink>& result) {
  base::hash_set<MediaSink::Id> result_sink_ids;
  for (const MediaSink& sink : result)
    result_sink_ids.insert(sink.id());

  // (1) Iterate through current sink set, remove cast mode from those that
  // do not appear in latest result.
  for (auto it = all_sinks_.begin(); it != all_sinks_.end(); /*no-op*/) {
    if (!ContainsKey(result_sink_ids, it->first)) {
      it->second.cast_modes.erase(cast_mode);
    }
    if (!IsValid(it->second)) {
      all_sinks_.erase(it++);
    } else {
      ++it;
    }
  }

  // (2) Add / update sinks with latest result.
  for (const MediaSink& sink : result) {
    auto result =
      all_sinks_.insert(std::make_pair(sink.id(),
                                       MediaSinkWithCastModes(sink)));
    if (!result.second)
      result.first->second.sink = sink;
    result.first->second.cast_modes.insert(cast_mode);
  }
}

void QueryResultManager::GetSupportedCastModes(CastModeSet* cast_modes) const {
  DCHECK(cast_modes);
  cast_modes->clear();
  for (const auto& observer_pair : sinks_observers_) {
    cast_modes->insert(observer_pair.first);
  }
}

MediaSource QueryResultManager::GetSourceForCastMode(
      MediaCastMode cast_mode) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto source_it = cast_mode_sources_.find(cast_mode);
  return source_it == cast_mode_sources_.end() ?
    MediaSource() : source_it->second;
}

void QueryResultManager::NotifyOnResultsUpdated() {
  std::vector<MediaSinkWithCastModes> sinks;
  for (const auto& sink_pair : all_sinks_) {
    sinks.push_back(sink_pair.second);
  }
  FOR_EACH_OBSERVER(QueryResultManager::Observer, observers_,
      OnResultsUpdated(sinks));
}

}  // namespace media_router
