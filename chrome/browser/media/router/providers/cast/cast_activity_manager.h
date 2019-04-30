// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_MANAGER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "chrome/common/media_router/route_request_result.h"
#include "components/cast_channel/cast_message_handler.h"
#include "components/cast_channel/cast_message_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"
#include "url/origin.h"

namespace cast_channel {
class CastMessage;
}

namespace media_router {

class CastActivityRecord;
class CastSession;
class DataDecoder;
class MediaSinkServiceBase;

// Handles launching and terminating Cast application on a Cast receiver, and
// acts as the source of truth for Cast activities and MediaRoutes.
class CastActivityManager : public cast_channel::CastMessageHandler::Observer,
                            public CastSessionTracker::Observer {
 public:
  // |media_sink_service|: Provides Cast MediaSinks.
  // |message_handler|: Used for sending and receiving messages to Cast
  // receivers.
  // |media_router|: Mojo ptr to MediaRouter.
  // |data_decoder|: Used for parsing JSON messages from Cast SDK and Cast
  // receivers.
  // |hash_token|: Used for hashing receiver IDs in messages sent to the Cast
  // SDK.
  CastActivityManager(MediaSinkServiceBase* media_sink_service,
                      CastSessionTracker* session_tracker,
                      cast_channel::CastMessageHandler* message_handler,
                      mojom::MediaRouter* media_router,
                      std::unique_ptr<DataDecoder> data_decoder,
                      const std::string& hash_token);
  ~CastActivityManager() override;

  // Adds or removes a route query with |source|. When adding a route query, if
  // the current list of routes is non-empty, the query will be immediately
  // updated with the current list.
  // TODO(https://crbug.com/882481): Simplify the route query API.
  void AddRouteQuery(const MediaSource::Id& source);
  void RemoveRouteQuery(const MediaSource::Id& source);

  // Launches a Cast session with parameters given by |cast_source| to |sink|.
  // Returns the created MediaRoute and notifies existing route queries.
  void LaunchSession(const CastMediaSource& cast_source,
                     const MediaSinkInternal& sink,
                     const std::string& presentation_id,
                     const url::Origin& origin,
                     int tab_id,
                     bool incognito,
                     mojom::MediaRouteProvider::CreateRouteCallback callback);

  void JoinSession(const CastMediaSource& cast_source,
                   const std::string& presentation_id,
                   const url::Origin& origin,
                   int tab_id,
                   bool incognito,
                   mojom::MediaRouteProvider::JoinRouteCallback callback);

  // Terminates a Cast session represented by |route_id|.
  void TerminateSession(
      const MediaRoute::Id& route_id,
      mojom::MediaRouteProvider::TerminateRouteCallback callback);

  const MediaRoute* GetRoute(const MediaRoute::Id& route_id) const;
  std::vector<MediaRoute> GetRoutes() const;

  // cast_channel::CastMessageHandler::Observer override.
  void OnAppMessage(int channel_id,
                    const cast_channel::CastMessage& message) override;

  // CastSessionTracker::Observer implementation.
  void OnSessionAddedOrUpdated(const MediaSinkInternal& sink,
                               const CastSession& session) override;
  void OnSessionRemoved(const MediaSinkInternal& sink) override;
  void OnMediaStatusUpdated(const MediaSinkInternal& sink,
                            const base::Value& media_status,
                            base::Optional<int> request_id) override;

 private:
  friend class CastActivityRecord;

  using ActivityMap =
      base::flat_map<MediaRoute::Id, std::unique_ptr<CastActivityRecord>>;

  // Bundle of parameters for DoLaunchSession().
  struct DoLaunchSessionParams {
    // Note: The compiler-generated constructors and destructor would be
    // sufficient here, but the style guide requires them to be defined
    // explicitly.
    DoLaunchSessionParams(
        const MediaRoute& route,
        const CastMediaSource& cast_source,
        const MediaSinkInternal& sink,
        const url::Origin& origin,
        int tab_id,
        mojom::MediaRouteProvider::CreateRouteCallback callback);
    DoLaunchSessionParams(DoLaunchSessionParams&& other);
    ~DoLaunchSessionParams();
    DoLaunchSessionParams& operator=(DoLaunchSessionParams&&) = delete;

    // The route for which a session is being launched.
    MediaRoute route;

    // The media source for which a session is being launched.
    CastMediaSource cast_source;

    // The sink for which a session is being launched.
    MediaSinkInternal sink;

    // The origin of the Cast SDK client. Used for auto-join.
    url::Origin origin;

    // The tab ID of the Cast SDK client. Used for auto-join.
    int tab_id;

    // Callback to execute after the launch request has been sent.
    mojom::MediaRouteProvider::CreateRouteCallback callback;
  };

  void DoLaunchSession(DoLaunchSessionParams params);
  void LaunchSessionAfterTerminatingExisting(
      const MediaRoute::Id& existing_route_id,
      DoLaunchSessionParams params,
      const base::Optional<std::string>& error_string,
      RouteRequestResult::ResultCode result);

  // Removes an activity, terminating any associated connections, then notifies
  // the media router that routes have been updated.
  void RemoveActivity(
      ActivityMap::iterator activity_it,
      blink::mojom::PresentationConnectionState state,
      blink::mojom::PresentationConnectionCloseReason close_reason);

  // Removes an activity without sending the usual notification.
  //
  // TODO(jrw): Figure out why it's desirable to avoid sending the usual
  // notification sometimes.
  void RemoveActivityWithoutNotification(
      ActivityMap::iterator activity_it,
      blink::mojom::PresentationConnectionState state,
      blink::mojom::PresentationConnectionCloseReason close_reason);

  void NotifyAllOnRoutesUpdated();
  void NotifyOnRoutesUpdated(const MediaSource::Id& source_id,
                             const std::vector<MediaRoute>& routes);

  void HandleLaunchSessionResponse(
      const MediaRoute::Id& route_id,
      const MediaSinkInternal& sink,
      const CastMediaSource& cast_source,
      cast_channel::LaunchSessionResponse response);
  void HandleStopSessionResponse(
      const MediaRoute::Id& route_id,
      mojom::MediaRouteProvider::TerminateRouteCallback callback,
      cast_channel::Result result);

  CastActivityRecord* FindActivityForAutoJoin(
      const CastMediaSource& cast_source,
      const url::Origin& origin,
      int tab_id);
  CastActivityRecord* FindActivityForSessionJoin(
      const CastMediaSource& cast_source,
      const std::string& presentation_id);
  bool CanJoinSession(const CastActivityRecord& activity,
                      const CastMediaSource& cast_source,
                      bool incognito) const;

  // Creates and stores a CastActivityRecord representing a non-local activity.
  void AddNonLocalActivityRecord(const MediaSinkInternal& sink,
                                 const CastSession& session);

  void SendFailedToCastIssue(const MediaSink::Id& sink_id,
                             const MediaRoute::Id& route_id);

  base::WeakPtr<CastActivityManager> GetWeakPtr();

  // These methods return |activities_.end()| when nothing is found.
  ActivityMap::iterator FindActivityByChannelId(int channel_id);
  ActivityMap::iterator FindActivityBySink(const MediaSinkInternal& sink);

  base::flat_set<MediaSource::Id> route_queries_;
  ActivityMap activities_;

  // The following raw pointer fields are assumed to outlive |this|.
  MediaSinkServiceBase* const media_sink_service_;
  CastSessionTracker* const session_tracker_;
  cast_channel::CastMessageHandler* const message_handler_;
  mojom::MediaRouter* const media_router_;

  const std::unique_ptr<DataDecoder> data_decoder_;
  const std::string hash_token_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastActivityManager> weak_ptr_factory_;
  FRIEND_TEST_ALL_PREFIXES(CastActivityManagerTest, SendMediaRequestToReceiver);
  DISALLOW_COPY_AND_ASSIGN(CastActivityManager);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_MANAGER_H_
