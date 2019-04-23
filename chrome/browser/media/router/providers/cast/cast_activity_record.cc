// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_record.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "url/origin.h"

using blink::mojom::PresentationConnectionCloseReason;
using blink::mojom::PresentationConnectionMessagePtr;

namespace media_router {

CastActivityRecord::~CastActivityRecord() {}

mojom::RoutePresentationConnectionPtr CastActivityRecord::AddClient(
    const CastMediaSource& source,
    const url::Origin& origin,
    int tab_id) {
  const std::string& client_id = source.client_id();
  auto client = std::make_unique<CastSessionClient>(client_id, origin, tab_id,
                                                    source.auto_join_policy(),
                                                    data_decoder_, this);
  auto presentation_connection = client->Init();
  connected_clients_.emplace(client_id, std::move(client));

  // Route is now local due to connected client.
  route_.set_local(true);
  return presentation_connection;
}

void CastActivityRecord::RemoveClient(const std::string& client_id) {
  // Don't erase by key here as the |client_id| may be referring to the
  // client being deleted.
  auto it = connected_clients_.find(client_id);
  if (it != connected_clients_.end())
    connected_clients_.erase(it);
}

void CastActivityRecord::SetOrUpdateSession(const CastSession& session,
                                            const MediaSinkInternal& sink,
                                            const std::string& hash_token) {
  DVLOG(2) << "CastActivityRecord::SetOrUpdateSession old session_id = "
           << session_id_.value_or("<missing>")
           << ", new session_id = " << session.session_id();
  if (!session_id_) {
    session_id_ = session.session_id();
  } else {
    DCHECK_EQ(*session_id_, session.session_id());
    for (auto& client : connected_clients_)
      client.second->SendMessageToClient(
          CreateUpdateSessionMessage(session, client.first, sink, hash_token));
  }
  route_.set_description(session.GetRouteDescription());
}

cast_channel::Result CastActivityRecord::SendAppMessageToReceiver(
    const CastInternalMessage& cast_message) {
  const CastSession* session = GetSession();
  if (!session)
    return cast_channel::Result::kFailed;  // TODO(jrw): Send error code
                                           // back to SDK client.
  const std::string& message_namespace = cast_message.app_message_namespace();
  if (!base::ContainsKey(session->message_namespaces(), message_namespace)) {
    DLOG(ERROR) << "Disallowed message namespace: " << message_namespace;
    // TODO(jrw): Send error code back to SDK client.
    return cast_channel::Result::kFailed;
  }
  return message_handler_->SendAppMessage(
      GetCastChannelId(),
      cast_channel::CreateCastMessage(
          message_namespace, cast_message.app_message_body(),
          cast_message.client_id, session->transport_id()));
}

base::Optional<int> CastActivityRecord::SendMediaRequestToReceiver(
    const CastInternalMessage& cast_message) {
  CastSession* session = GetSession();
  if (!session)
    return base::nullopt;
  return message_handler_->SendMediaRequest(
      GetCastChannelId(), cast_message.v2_message_body(),
      cast_message.client_id, session->transport_id());
}

void CastActivityRecord::SendSetVolumeRequestToReceiver(
    const CastInternalMessage& cast_message,
    cast_channel::ResultCallback callback) {
  message_handler_->SendSetVolumeRequest(
      GetCastChannelId(), cast_message.v2_message_body(),
      cast_message.client_id, std::move(callback));
}

void CastActivityRecord::SendStopSessionMessageToReceiver(
    const base::Optional<std::string>& client_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback) {
  const std::string& sink_id = route_.media_sink_id();
  const MediaSinkInternal* sink = media_sink_service_->GetSinkById(sink_id);
  DCHECK(sink);
  DCHECK(session_id_);

  message_handler_->StopSession(
      sink->cast_data().cast_channel_id, *session_id_, client_id,
      base::BindOnce(&CastActivityManager::HandleStopSessionResponse,
                     activity_manager_->GetWeakPtr(), route_.media_route_id(),
                     std::move(callback)));
}

void CastActivityRecord::HandleLeaveSession(const std::string& client_id) {
  auto client_it = connected_clients_.find(client_id);
  CHECK(client_it != connected_clients_.end());
  CastSessionClient& client = *client_it->second;
  std::vector<std::string> leaving_client_ids;
  for (const auto& pair : connected_clients_) {
    if (pair.second->MatchesAutoJoinPolicy(client.origin(), client.tab_id()))
      leaving_client_ids.push_back(pair.first);
  }

  for (const auto& client_id : leaving_client_ids) {
    auto leaving_client_it = connected_clients_.find(client_id);
    CHECK(leaving_client_it != connected_clients_.end());
    leaving_client_it->second->CloseConnection(
        PresentationConnectionCloseReason::CLOSED);
    connected_clients_.erase(leaving_client_it);
  }
}

void CastActivityRecord::SendMessageToClient(
    const std::string& client_id,
    PresentationConnectionMessagePtr message) {
  auto it = connected_clients_.find(client_id);
  if (it == connected_clients_.end()) {
    DLOG(ERROR) << "Attempting to send message to nonexistent client: "
                << client_id;
    return;
  }
  it->second->SendMessageToClient(std::move(message));
}

void CastActivityRecord::ClosePresentationConnections(
    PresentationConnectionCloseReason close_reason) {
  for (auto& client : connected_clients_)
    client.second->CloseConnection(close_reason);
}

void CastActivityRecord::TerminatePresentationConnections() {
  for (auto& client : connected_clients_)
    client.second->TerminateConnection();
}

CastActivityRecord::CastActivityRecord(
    const MediaRoute& route,
    const std::string& app_id,
    MediaSinkServiceBase* media_sink_service,
    cast_channel::CastMessageHandler* message_handler,
    CastSessionTracker* session_tracker,
    DataDecoder* data_decoder,
    CastActivityManager* owner)
    : route_(route),
      app_id_(app_id),
      media_sink_service_(media_sink_service),
      message_handler_(message_handler),
      session_tracker_(session_tracker),
      data_decoder_(data_decoder),
      activity_manager_(owner) {}

CastSession* CastActivityRecord::GetSession() {
  DCHECK(session_id_);
  CastSession* session = session_tracker_->GetSessionById(*session_id_);
  if (!session) {
    // TODO(crbug.com/905002): Add UMA metrics for this and other error
    // conditions.
    LOG(ERROR) << "Session not found: " << session_id_.value_or("<missing>");
  }
  return session;
}

int CastActivityRecord::GetCastChannelId() {
  const MediaSinkInternal* sink = media_sink_service_->GetSinkByRoute(route_);
  if (!sink) {
    // TODO(crbug.com/905002): Add UMA metrics for this and other error
    // conditions.
    LOG(ERROR) << "Sink not found for route: " << route_;
    return -1;
  }
  return sink->cast_data().cast_channel_id;
}

}  // namespace media_router
