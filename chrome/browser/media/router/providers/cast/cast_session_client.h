// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_SESSION_CLIENT_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_SESSION_CLIENT_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "components/cast_channel/cast_message_handler.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"
#include "url/origin.h"

namespace media_router {

class CastActivityRecord;
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
  //
  // TODO(jrw): Remove redundant "ToClient" in the name of this and other
  // methods.
  void SendMessageToClient(
      blink::mojom::PresentationConnectionMessagePtr message);

  // Sends a media status message to the client.  If |request_id| is given, it
  // is used to look up the sequence number of a previous request, which is
  // included in the outgoing message.
  void SendMediaStatusToClient(const base::Value& media_status,
                               base::Optional<int> request_id);

  // Changes the PresentationConnection state to CLOSED/TERMINATED and resets
  // PresentationConnection message pipes.
  void CloseConnection(
      blink::mojom::PresentationConnectionCloseReason close_reason);
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

  void SendErrorCodeToClient(int sequence_number,
                             CastInternalMessage::ErrorCode error_code,
                             base::Optional<std::string> description);

  // NOTE: This is current only called from SendErrorCodeToClient, but based on
  // the old code this method based on, it seems likely it will have other
  // callers once error handling for the Cast MRP is more fleshed out.
  void SendErrorToClient(int sequence_number, base::Value error);

 private:
  void HandleParsedClientMessage(std::unique_ptr<base::Value> message);
  void HandleV2ProtocolMessage(const CastInternalMessage& cast_message);

  // Resets the PresentationConnection Mojo message pipes.
  void TearDownPresentationConnection();

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

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_SESSION_CLIENT_H_
