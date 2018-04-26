// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_APP_DISCOVERY_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_APP_DISCOVERY_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/providers/cast/cast_app_availability_tracker.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "components/cast_channel/cast_message_util.h"

namespace base {
class TickClock;
}

namespace cast_channel {
class CastMessageHandler;
class CastSocket;
}  // namespace cast_channel

namespace media_router {

// Keeps track of sink queries and listens to CastMediaSinkServiceImpl for sink
// updates, and issues app availability requests based on these signals. This
// class may be created on any sequence. All other methods must be called on the
// CastSocketService sequence.
class CastAppDiscoveryService : public CastMediaSinkServiceImpl::Observer {
 public:
  using SinkQueryFunc = void(const MediaSource::Id& source_id,
                             const std::vector<MediaSinkInternal>& sinks);
  using SinkQueryCallback = base::RepeatingCallback<SinkQueryFunc>;
  using SinkQueryCallbackList = base::CallbackList<SinkQueryFunc>;
  using Subscription = std::unique_ptr<SinkQueryCallbackList::Subscription>;

  CastAppDiscoveryService(cast_channel::CastMessageHandler* message_handler,
                          cast_channel::CastSocketService* socket_service,
                          const base::TickClock* clock);
  ~CastAppDiscoveryService() override;

  // Adds a sink query for |source|. Results will be continuously returned via
  // |callback| until the returned Subscription is destroyed by the caller.
  // If there are cached results available, |callback| will be invoked before
  // this method returns.
  Subscription StartObservingMediaSinks(const CastMediaSource& source,
                                        const SinkQueryCallback& callback);

  // Reissues app availability requests for currently registered (sink, app_id)
  // pairs whose status is kUnavailable or kUnknown. It is suitable to call
  // this method when the user initiates a user gesture (such as opening the
  // Media Router dialog).
  void Refresh();

 private:
  friend class CastAppDiscoveryServiceTest;

  // CastMediaSinkServiceImpl::Observer
  void OnSinkAddedOrUpdated(const MediaSinkInternal& sink,
                            cast_channel::CastSocket* socket) override;
  void OnSinkRemoved(const MediaSinkInternal& sink) override;

  // Issues an app availability request for |app_id| to the sink given by
  // |sink_id| via |socket|.
  void RequestAppAvailability(cast_channel::CastSocket* socket,
                              const std::string& app_id,
                              const MediaSink::Id& sink_id);

  // Updates the availability result for |sink_id| and |app_id| with |result|,
  // and notifies callbacks with updated sink query results.
  // |start_time| is the time when the app availability request was made, and
  // is used for metrics.
  void UpdateAppAvailability(base::TimeTicks start_time,
                             const MediaSink::Id& sink_id,
                             const std::string& app_id,
                             cast_channel::GetAppAvailabilityResult result);

  // Updates the sink query results for |sources|.
  void UpdateSinkQueries(const std::vector<CastMediaSource>& sources);

  // Removes the entry from |sink_queries_| if there are no more callbacks
  // associated with |source|.
  void MaybeRemoveSinkQueryEntry(const CastMediaSource& source);

  // Gets a list of sinks corresponding to |sink_ids|.
  std::vector<MediaSinkInternal> GetSinksByIds(
      const base::flat_set<MediaSink::Id>& sink_ids) const;

  // Registered sink queries and their associated callbacks.
  base::flat_map<MediaSource::Id, std::unique_ptr<SinkQueryCallbackList>>
      sink_queries_;

  cast_channel::CastMessageHandler* const message_handler_;
  cast_channel::CastSocketService* const socket_service_;

  CastAppAvailabilityTracker availability_tracker_;
  base::flat_map<MediaSink::Id, MediaSinkInternal> sinks_;

  const base::TickClock* const clock_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastAppDiscoveryService> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(CastAppDiscoveryService);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_APP_DISCOVERY_SERVICE_H_
