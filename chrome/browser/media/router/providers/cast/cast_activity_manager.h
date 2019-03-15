// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_MANAGER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/discovery/media_sink_service_base.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "components/cast_channel/cast_message_handler.h"
#include "components/cast_channel/cast_message_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace media_router {

class CastActivityManager;
class CastActivityRecord;
class CastMediaSource;
class DataDecoder;

// Represents a Cast SDK client connection to a Cast session. This class
// contains PresentationConnection Mojo pipes to send and receive messages
// from/to the corresponding SDK client hosted in a presentation controlling
// frame in Blink.
class CastSessionClient : public blink::mojom::PresentationConnection {
 public:
  CastSessionClient(const std::string& client_id,
                    const url::Origin& origin,
                    int tab_id,
                    AutoJoinPolicy auto_join_policy,
                    DataDecoder* data_decoder,
                    CastActivityRecord* activity);
  ~CastSessionClient() override;

  const std::string& client_id() const { return client_id_; }
  const base::Optional<std::string>& session_id() const { return session_id_; }
  const url::Origin& origin() const { return origin_; }
  int tab_id() { return tab_id_; }

  // Initializes the PresentationConnection Mojo message pipes and returns the
  // handles of the two pipes to be held by Blink. Also transitions the
  // connection state to CONNECTED. This method can only be called once, and
  // must be called before |SendMessageToClient()|.
  mojom::RoutePresentationConnectionPtr Init();

  // Sends |message| to the Cast SDK client in Blink.
  void SendMessageToClient(
      blink::mojom::PresentationConnectionMessagePtr message);

  // Sends a media status message to the client.  If |request_id| is given, it
  // is used to look up the sequence number of a previous request, which is
  // included in the outgoing message.
  void SendMediaStatusToClient(const base::Value& media_status,
                               base::Optional<int> request_id);

  // Changes the PresentationConnection state to CLOSED/TERMINATED and resets
  // PresentationConnection message pipes.
  void CloseConnection();
  void TerminateConnection();

  // blink::mojom::PresentationConnection implementation
  void OnMessage(
      blink::mojom::PresentationConnectionMessagePtr message) override;
  // Blink does not initiate state change or close using PresentationConnection.
  // Instead, |PresentationService::Close/TerminateConnection| is used.
  void DidChangeState(
      blink::mojom::PresentationConnectionState state) override {}
  void DidClose(
      blink::mojom::PresentationConnectionCloseReason reason) override;

  // Tests whether the specified origin and tab ID match this session's origin
  // and tab ID to the extent required by this sesssion's auto-join policy.
  // Depending on the value of |auto_join_policy_|, |origin|, |tab_id|, or both
  // may be ignored.
  //
  // TODO(jrw): It appears the real purpose of this method is to detect whether
  // this session was created by an auto-join request, but auto-joining isn't
  // implemented yet.  This comment should probably be updated once auto-join is
  // implemented and I've verified this method does what I think it does.
  // Alternatively, it might make more sense to record at session creation time
  // whether a particular session was created by an auto-join request, in which
  // case I believe this method would no longer be needed.
  bool MatchesAutoJoinPolicy(url::Origin origin, int tab_id) const;

 private:
  void HandleParsedClientMessage(std::unique_ptr<base::Value> message);
  void HandleV2ProtocolMessage(const CastInternalMessage& cast_message);

  // Sends a response to the client indicating that a particular request
  // succeeded or failed.
  void SendResultResponse(int sequence_number, cast_channel::Result result);

  std::string client_id_;
  base::Optional<std::string> session_id_;

  // The origin and tab ID parameters originally passed to the CreateRoute
  // method of the MediaRouteProvider Mojo interface.
  url::Origin origin_;
  int tab_id_;

  const AutoJoinPolicy auto_join_policy_;

  DataDecoder* const data_decoder_;
  CastActivityRecord* const activity_;

  // The maximum number of pending media requests, used to prevent memory leaks.
  // Normally the number of pending requests should be fairly small, but each
  // entry only consumes 2*sizeof(int) bytes, so the upper limit is set fairly
  // high.
  static constexpr std::size_t kMaxPendingMediaRequests = 1024;

  // Maps internal, locally-generated request IDs to sequence numbers from cast
  // messages received from the client.  Used to set an appropriate
  // sequenceNumber field in outgoing messages so a client can associate a media
  // status message with a previous request.
  //
  // TODO(jrw): Investigate whether this mapping is really necessary, or if
  // sequence numbers can be used directly without generating request IDs.
  base::flat_map<int, int> pending_media_requests_;

  // Binding for the PresentationConnection in Blink to receive incoming
  // messages and respond to state changes.
  mojo::Binding<blink::mojom::PresentationConnection> connection_binding_;

  // Mojo message pipe to PresentationConnection in Blink to send messages and
  // initiate state changes.
  blink::mojom::PresentationConnectionPtr connection_;

  base::WeakPtrFactory<CastSessionClient> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(CastSessionClient);
};

// Represents an ongoing or launching Cast application on a Cast receiver.
// It keeps track of the set of Cast SDK clients connected to the application.
// Note that we do not keep track of 1-UA mode presentations here. Instead, they
// are handled by LocalPresentationManager.
//
// Instances of this class are associated with a specific session and app.
class CastActivityRecord {
 public:
  ~CastActivityRecord();

  const MediaRoute& route() const { return route_; }
  const std::string& app_id() const { return app_id_; }
  const base::flat_map<std::string, std::unique_ptr<CastSessionClient>>&
  connected_clients() const {
    return connected_clients_;
  }
  const base::Optional<std::string>& session_id() const { return session_id_; }

  // Sends app message |cast_message|, which came from the SDK client, to the
  // receiver hosting this session. Returns true if the message is sent
  // successfully.
  cast_channel::Result SendAppMessageToReceiver(
      const CastInternalMessage& cast_message);

  // Sends media command |cast_message|, which came from the SDK client, to the
  // receiver hosting this session. Returns the locally-assigned request ID of
  // the message sent to the receiver.
  base::Optional<int> SendMediaRequestToReceiver(
      const CastInternalMessage& cast_message);

  // Sends a SET_VOLUME request to the receiver and calls |callback| when a
  // response indicating whether the request succeeded is received.
  void SendSetVolumeRequestToReceiver(const CastInternalMessage& cast_message,
                                      cast_channel::ResultCallback callback);

  void SendStopSessionMessageToReceiver(
      const base::Optional<std::string>& client_id,
      mojom::MediaRouteProvider::TerminateRouteCallback callback);

  // Called when the client given by |client_id| requests to leave the session.
  // This will also cause all clients within the session with matching origin
  // and/or tab ID to leave (i.e., their presentation connections will be
  // closed).
  void HandleLeaveSession(const std::string& client_id);

  // Adds a new client |client_id| to this session and returns the handles of
  // the two pipes to be held by Blink It is invalid to call this method if the
  // client already exists.
  mojom::RoutePresentationConnectionPtr AddClient(
      const std::string& client_id,
      const url::Origin& origin,
      int tab_id,
      AutoJoinPolicy auto_join_policy);

  // On the first call, saves the ID of |session|.  On subsequent calls,
  // notifies all connected clients that the session has been updated.  In both
  // cases, the stored route description is updated to match the session
  // description.
  //
  // The |hash_token| parameter is used for hashing receiver IDs in messages
  // sent to the Cast SDK, and |sink| is the sink associated with |session|.
  void SetOrUpdateSession(const CastSession& session,
                          const MediaSinkInternal& sink,
                          const std::string& hash_token);

  // Sends |message| to the client given by |client_id|.
  void SendMessageToClient(
      const std::string& client_id,
      blink::mojom::PresentationConnectionMessagePtr message);

  // Closes / Terminates the PresentationConnections of all clients connected
  // to this activity.
  void ClosePresentationConnections();
  void TerminatePresentationConnections();

 private:
  friend class CastSessionClient;
  friend class CastActivityManager;

  // Creates a new record owned by |owner|.
  CastActivityRecord(const MediaRoute& route,
                     const std::string& app_id,
                     MediaSinkServiceBase* media_sink_service,
                     cast_channel::CastMessageHandler* message_handler,
                     CastSessionTracker* session_tracker,
                     DataDecoder* data_decoder,
                     CastActivityManager* owner);

  CastSession* GetSession();
  int GetCastChannelId();

  MediaRoute route_;
  const std::string app_id_;
  base::flat_map<std::string, std::unique_ptr<CastSessionClient>>
      connected_clients_;

  // Set by CastActivityManager after the session is launched successfully.
  base::Optional<std::string> session_id_;

  MediaSinkServiceBase* const media_sink_service_;
  // TODO(https://crbug.com/809249): Consider wrapping CastMessageHandler with
  // known parameters (sink, client ID, session transport ID) and passing them
  // to objects that need to send messges to the receiver.
  cast_channel::CastMessageHandler* const message_handler_;
  CastSessionTracker* const session_tracker_;
  DataDecoder* const data_decoder_;
  CastActivityManager* const activity_manager_;

  DISALLOW_COPY_AND_ASSIGN(CastActivityRecord);
};

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

  void RemoveActivity(ActivityMap::iterator activity_it,
                      blink::mojom::PresentationConnectionState state =
                          blink::mojom::PresentationConnectionState::TERMINATED,
                      bool notify = true);
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
