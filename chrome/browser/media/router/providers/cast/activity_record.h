// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_ACTIVITY_RECORD_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_ACTIVITY_RECORD_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace cast_channel {
class CastMessageHandler;
}

namespace media_router {

class CastActivityRecord;
class MirroringActivityRecord;
class CastSessionTracker;

class ActivityRecordFactoryForTest {
 public:
  virtual std::unique_ptr<CastActivityRecord> MakeCastActivityRecord(
      const MediaRoute& route,
      const std::string& app_id) = 0;
  virtual std::unique_ptr<MirroringActivityRecord> MakeMirroringActivityRecord(
      const MediaRoute& route,
      const std::string& app_id) = 0;
};

class ActivityRecord {
 public:
  ActivityRecord(const MediaRoute& route,
                 const std::string& app_id,
                 cast_channel::CastMessageHandler* message_handler,
                 CastSessionTracker* session_tracker);
  ActivityRecord(const ActivityRecord&) = delete;
  ActivityRecord& operator=(const ActivityRecord&) = delete;
  virtual ~ActivityRecord();

  const MediaRoute& route() const { return route_; }
  const std::string& app_id() const { return app_id_; }
  const base::Optional<std::string>& session_id() const { return session_id_; }
  base::Optional<int> mirroring_tab_id() const { return mirroring_tab_id_; }
  const MediaSinkInternal sink() const { return sink_; }

  // Adds a new client |client_id| to this session and returns the handles of
  // the two pipes to be held by Blink It is invalid to call this method if the
  // client already exists.
  virtual mojom::RoutePresentationConnectionPtr AddClient(
      const CastMediaSource& source,
      const url::Origin& origin,
      int tab_id);

  virtual void RemoveClient(const std::string& client_id);

  // On the first call, saves the ID of |session|.  On subsequent calls,
  // notifies all connected clients that the session has been updated.  In both
  // cases, the stored route description is updated to match the session
  // description.
  //
  // The |hash_token| parameter is used for hashing receiver IDs in messages
  // sent to the Cast SDK, and |sink| is the sink associated with |session|.
  virtual void SetOrUpdateSession(const CastSession& session,
                                  const MediaSinkInternal& sink,
                                  const std::string& hash_token);

  virtual void SendStopSessionMessageToClients(const std::string& hash_token);

  // Sends |message| to the client given by |client_id|.
  //
  // TODO(jrw): This method's functionality overlaps that of OnAppMessage().
  // Can the methods be combined?
  virtual void SendMessageToClient(
      const std::string& client_id,
      blink::mojom::PresentationConnectionMessagePtr message);

  virtual void SendMediaStatusToClients(const base::Value& media_status,
                                        base::Optional<int> request_id);

  // Handles a message forwarded by CastActivityManager.
  virtual void OnAppMessage(const cast::channel::CastMessage& message) = 0;
  virtual void OnInternalMessage(
      const cast_channel::InternalMessage& message) = 0;

  // Closes/terminates the PresentationConnections of all clients connected to
  // this activity.
  virtual void ClosePresentationConnections(
      blink::mojom::PresentationConnectionCloseReason close_reason);
  virtual void TerminatePresentationConnections();

  virtual void CreateMediaController(
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) = 0;

  // Sends media command |cast_message|, which came from the SDK client, to the
  // receiver hosting this session. Returns the locally-assigned request ID of
  // the message sent to the receiver.
  virtual base::Optional<int> SendMediaRequestToReceiver(
      const CastInternalMessage& cast_message);

  // Sends app message |cast_message|, which came from the SDK client, to the
  // receiver hosting this session. Returns true if the message is sent
  // successfully.
  virtual cast_channel::Result SendAppMessageToReceiver(
      const CastInternalMessage& cast_message);

  // Sends a SET_VOLUME request to the receiver and calls |callback| when a
  // response indicating whether the request succeeded is received.
  virtual void SendSetVolumeRequestToReceiver(
      const CastInternalMessage& cast_message,
      cast_channel::ResultCallback callback);

  // Stops the currently active session on the receiver, and invokes |callback|
  // with the result. Called when a SDK client requests to stop the session.
  virtual void StopSessionOnReceiver(const std::string& client_id,
                                     cast_channel::ResultCallback callback);

  // Closes any virtual connection between |client_id| and this session on the
  // receiver.
  virtual void CloseConnectionOnReceiver(const std::string& client_id);

  // Called when the client given by |client_id| requests to leave the session.
  // This will also cause all clients within the session with matching origin
  // and/or tab ID to leave (i.e., their presentation connections will be
  // closed).
  virtual void HandleLeaveSession(const std::string& client_id);

  static void SetClientFactoryForTest(
      CastSessionClientFactoryForTest* factory) {
    client_factory_for_test_ = factory;
  }

 protected:
  using ClientMap =
      base::flat_map<std::string, std::unique_ptr<CastSessionClient>>;

  // Gets the session associated with this activity.  May return nullptr.
  CastSession* GetSession() const;

  CastSessionClient* GetClient(const std::string& client_id) {
    auto it = connected_clients_.find(client_id);
    return it == connected_clients_.end() ? nullptr : it->second.get();
  }

  int cast_channel_id() const { return sink_.cast_channel_id(); }

  MediaRoute route_;
  std::string app_id_;
  base::Optional<int> mirroring_tab_id_;

  // Called when a session is initially set from SetOrUpdateSession().
  base::OnceCallback<void()> on_session_set_;

  // TODO(https://crbug.com/809249): Consider wrapping CastMessageHandler with
  // known parameters (sink, client ID, session transport ID) and passing them
  // to objects that need to send messages to the receiver.
  cast_channel::CastMessageHandler* const message_handler_;

  CastSessionTracker* const session_tracker_;

  // Set by CastActivityManager after the session is launched successfully.
  base::Optional<std::string> session_id_;

  MediaSinkInternal sink_;
  ClientMap connected_clients_;

 private:
  friend class CastActivityRecordTest;

  static CastSessionClientFactoryForTest* client_factory_for_test_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_ACTIVITY_RECORD_H_
