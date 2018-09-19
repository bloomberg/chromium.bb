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
#include "base/sequence_checker.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "components/cast_channel/cast_message_handler.h"
#include "components/cast_channel/cast_message_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace media_router {

class CastActivityManager;
class CastActivityRecord;
class CastMediaSource;
class DataDecoder;
class MediaSinkServiceBase;

// Represents a Cast SDK client connection to a Cast session. This class
// contains PresentationConnection Mojo pipes to send and receive messages
// from/to the corresponding SDK client hosted in a presentation controlling
// frame in Blink.
class CastSessionClient : public blink::mojom::PresentationConnection {
 public:
  CastSessionClient(const std::string& client_id,
                    const url::Origin& origin,
                    int tab_id,
                    DataDecoder* data_decoder,
                    CastActivityRecord* activity);
  ~CastSessionClient() override;

  const std::string& client_id() const { return client_id_; }
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

 private:
  void HandleParsedClientMessage(std::unique_ptr<base::Value> message);

  std::string client_id_;

  // The origin of the Cast SDK client. Used for auto-join.
  url::Origin origin_;

  // The tab ID of the Cast SDK client. Used for auto-join.
  int tab_id_;

  DataDecoder* const data_decoder_;
  CastActivityRecord* const activity_;

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
class CastActivityRecord {
 public:
  CastActivityRecord(const MediaRoute& route,
                     MediaSinkServiceBase* media_sink_service,
                     cast_channel::CastMessageHandler* message_handler,
                     DataDecoder* data_decoder);
  ~CastActivityRecord();

  const MediaRoute& route() const { return route_; }
  const base::flat_map<std::string, std::unique_ptr<CastSessionClient>>&
  connected_clients() {
    return connected_clients_;
  }
  const CastSession* session() { return session_.get(); }

  // Sends |cast_message|, which must be of type kAppMessage, to the receiver
  // hosting this session. Returns true if the message is sent successfully.
  bool SendAppMessageToReceiver(const CastInternalMessage& cast_message);

  // Adds a new client |client_id| to this session and returns the handles of
  // the two pipes to be held by Blink It is invalid to call this method if the
  // client already exists.
  mojom::RoutePresentationConnectionPtr AddClient(const std::string& client_id,
                                                  const url::Origin& origin,
                                                  int tab_id);

  // Also updates the description on |route_|.
  void SetSession(std::unique_ptr<CastSession> session);

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

  MediaRoute route_;
  base::flat_map<std::string, std::unique_ptr<CastSessionClient>>
      connected_clients_;

  // Set by CastActivityManager after the session is launched successfully.
  // TODO(imcheng): Change this to a session ID when sessions are tracked
  // by the CastSessionTracker singleton.
  std::unique_ptr<CastSession> session_;

  MediaSinkServiceBase* const media_sink_service_;
  // TODO(https://crbug.com/809249): Consider wrapping CastMessageHandler with
  // known parameters (sink, client ID, session transport ID) and passing them
  // to objects that need to send messges to the receiver.
  cast_channel::CastMessageHandler* const message_handler_;
  DataDecoder* const data_decoder_;

  DISALLOW_COPY_AND_ASSIGN(CastActivityRecord);
};

// Handles launching and terminating Cast application on a Cast receiver, and
// acts as the source of truth for Cast activities and MediaRoutes.
// TODO(imcheng): CastActivityManager should listen for RECEIVER_STATUS and
// MEDIA_STATUS messages to generate non-local routes and update existing
// routes.
class CastActivityManager : public cast_channel::CastMessageHandler::Observer {
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

 private:
  friend class CastActivityRecord;
  using ActivityMap =
      base::flat_map<MediaRoute::Id, std::unique_ptr<CastActivityRecord>>;

  void RemoveActivity(
      ActivityMap::iterator activity_it,
      blink::mojom::PresentationConnectionState state =
          blink::mojom::PresentationConnectionState::TERMINATED);
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
      bool success);

  void SendFailedToCastIssue(const MediaSink::Id& sink_id,
                             const MediaRoute::Id& route_id);

  ActivityMap::iterator GetActivityByChannelId(int channel_id);

  base::flat_set<MediaSource::Id> route_queries_;
  ActivityMap activities_;

  // The following raw pointer fields are assumed to outlive |this|.
  MediaSinkServiceBase* const media_sink_service_;
  cast_channel::CastMessageHandler* const message_handler_;
  mojom::MediaRouter* const media_router_;

  const std::unique_ptr<DataDecoder> data_decoder_;
  const std::string hash_token_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastActivityManager> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(CastActivityManager);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_MANAGER_H_
