// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_RECORD_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_RECORD_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "components/cast_channel/cast_message_handler.h"
#include "url/origin.h"

namespace media_router {

class CastActivityManagerBase;
class CastActivityRecord;
class CastSessionClientFactory;
class CastInternalMessage;
class CastSession;
class CastSessionClientBase;
class CastSessionClientFactory;
class CastSessionTracker;
class DataDecoder;
class MediaSinkServiceBase;

// TODO(jrw): Rename
//   CastActivityRecordBase -> CastActivityRecord
//   CastActivityRecord -> CastActivityRecordImpl
class CastActivityRecordBase {
 public:
  using ClientMap =
      base::flat_map<std::string, std::unique_ptr<CastSessionClientBase>>;

  CastActivityRecordBase(const MediaRoute& route, const std::string& app_id);
  virtual ~CastActivityRecordBase();

  const MediaRoute& route() const { return route_; }
  const std::string& app_id() const { return app_id_; }
  const ClientMap& connected_clients() const { return connected_clients_; }
  const base::Optional<std::string>& session_id() const { return session_id_; }

  // Sends app message |cast_message|, which came from the SDK client, to the
  // receiver hosting this session. Returns true if the message is sent
  // successfully.
  virtual cast_channel::Result SendAppMessageToReceiver(
      const CastInternalMessage& cast_message) = 0;

  // Sends media command |cast_message|, which came from the SDK client, to the
  // receiver hosting this session. Returns the locally-assigned request ID of
  // the message sent to the receiver.
  virtual base::Optional<int> SendMediaRequestToReceiver(
      const CastInternalMessage& cast_message) = 0;

  // Sends a SET_VOLUME request to the receiver and calls |callback| when a
  // response indicating whether the request succeeded is received.
  virtual void SendSetVolumeRequestToReceiver(
      const CastInternalMessage& cast_message,
      cast_channel::ResultCallback callback) = 0;

  virtual void SendStopSessionMessageToReceiver(
      const base::Optional<std::string>& client_id,
      mojom::MediaRouteProvider::TerminateRouteCallback callback) = 0;

  // Called when the client given by |client_id| requests to leave the session.
  // This will also cause all clients within the session with matching origin
  // and/or tab ID to leave (i.e., their presentation connections will be
  // closed).
  virtual void HandleLeaveSession(const std::string& client_id) = 0;

  // Adds a new client |client_id| to this session and returns the handles of
  // the two pipes to be held by Blink It is invalid to call this method if the
  // client already exists.
  virtual mojom::RoutePresentationConnectionPtr AddClient(
      const CastMediaSource& source,
      const url::Origin& origin,
      int tab_id) = 0;

  virtual void RemoveClient(const std::string& client_id) = 0;

  // On the first call, saves the ID of |session|.  On subsequent calls,
  // notifies all connected clients that the session has been updated.  In both
  // cases, the stored route description is updated to match the session
  // description.
  //
  // The |hash_token| parameter is used for hashing receiver IDs in messages
  // sent to the Cast SDK, and |sink| is the sink associated with |session|.
  virtual void SetOrUpdateSession(const CastSession& session,
                                  const MediaSinkInternal& sink,
                                  const std::string& hash_token) = 0;

  // Sends |message| to the client given by |client_id|.
  virtual void SendMessageToClient(
      const std::string& client_id,
      blink::mojom::PresentationConnectionMessagePtr message) = 0;

  // Closes / Terminates the PresentationConnections of all clients connected
  // to this activity.
  virtual void ClosePresentationConnections(
      blink::mojom::PresentationConnectionCloseReason close_reason) = 0;
  virtual void TerminatePresentationConnections() = 0;

 protected:
  MediaRoute route_;
  const std::string app_id_;
  ClientMap connected_clients_;

  // Set by CastActivityManager after the session is launched successfully.
  base::Optional<std::string> session_id_;
};

class CastActivityRecordFactory {
 public:
  virtual CastActivityRecordBase* MakeCastActivityRecord(
      const MediaRoute& route,
      const std::string& app_id) = 0;
};

// Represents an ongoing or launching Cast application on a Cast receiver.
// It keeps track of the set of Cast SDK clients connected to the application.
// Note that we do not keep track of 1-UA mode presentations here. Instead, they
// are handled by LocalPresentationManager.
//
// Instances of this class are associated with a specific session and app.
class CastActivityRecord : public CastActivityRecordBase {
 public:
  ~CastActivityRecord() override;

  // CastActivityRecordBase implementation
  cast_channel::Result SendAppMessageToReceiver(
      const CastInternalMessage& cast_message) override;
  base::Optional<int> SendMediaRequestToReceiver(
      const CastInternalMessage& cast_message) override;
  void SendSetVolumeRequestToReceiver(
      const CastInternalMessage& cast_message,
      cast_channel::ResultCallback callback) override;
  void SendStopSessionMessageToReceiver(
      const base::Optional<std::string>& client_id,
      mojom::MediaRouteProvider::TerminateRouteCallback callback) override;
  void HandleLeaveSession(const std::string& client_id) override;
  mojom::RoutePresentationConnectionPtr AddClient(const CastMediaSource& source,
                                                  const url::Origin& origin,
                                                  int tab_id) override;
  void RemoveClient(const std::string& client_id) override;
  void SetOrUpdateSession(const CastSession& session,
                          const MediaSinkInternal& sink,
                          const std::string& hash_token) override;
  void SendMessageToClient(
      const std::string& client_id,
      blink::mojom::PresentationConnectionMessagePtr message) override;
  void ClosePresentationConnections(
      blink::mojom::PresentationConnectionCloseReason close_reason) override;
  void TerminatePresentationConnections() override;

  static void SetClientFactoryForTest(CastSessionClientFactory* factory) {
    client_factory_ = factory;
  }

 private:
  friend class CastSessionClient;
  friend class CastActivityManager;
  friend class CastActivityRecordTest;

  // Creates a new record owned by |owner|.
  CastActivityRecord(const MediaRoute& route,
                     const std::string& app_id,
                     MediaSinkServiceBase* media_sink_service,
                     cast_channel::CastMessageHandler* message_handler,
                     CastSessionTracker* session_tracker,
                     DataDecoder* data_decoder,
                     CastActivityManagerBase* owner);

  CastSession* GetSession();
  int GetCastChannelId();

  CastSessionClientBase* GetClient(const std::string& client_id) {
    auto it = connected_clients_.find(client_id);
    return it == connected_clients_.end() ? nullptr : it->second.get();
  }

  static CastSessionClientFactory* client_factory_;

  MediaSinkServiceBase* const media_sink_service_;
  // TODO(https://crbug.com/809249): Consider wrapping CastMessageHandler with
  // known parameters (sink, client ID, session transport ID) and passing them
  // to objects that need to send messages to the receiver.
  cast_channel::CastMessageHandler* const message_handler_;
  CastSessionTracker* const session_tracker_;
  DataDecoder* const data_decoder_;
  CastActivityManagerBase* const activity_manager_;

  DISALLOW_COPY_AND_ASSIGN(CastActivityRecord);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_RECORD_H_
