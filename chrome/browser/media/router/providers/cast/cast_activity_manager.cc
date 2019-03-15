// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/common/media_router/discovery/media_sink_service_base.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"

namespace media_router {

namespace {

void ReportClientMessageParseError(const MediaRoute::Id& route_id,
                                   const std::string& error) {
  // TODO(crbug.com/808720): Record UMA metric for parse result.
  DVLOG(2) << "Failed to parse Cast client message for " << route_id << ": "
           << error;
}

}  // namespace

CastSessionClient::CastSessionClient(const std::string& client_id,
                                     const url::Origin& origin,
                                     int tab_id,
                                     AutoJoinPolicy auto_join_policy,
                                     DataDecoder* data_decoder,
                                     CastActivityRecord* activity)
    : client_id_(client_id),
      origin_(origin),
      tab_id_(tab_id),
      auto_join_policy_(auto_join_policy),
      data_decoder_(data_decoder),
      activity_(activity),
      connection_binding_(this),
      weak_ptr_factory_(this) {}

CastSessionClient::~CastSessionClient() = default;

mojom::RoutePresentationConnectionPtr CastSessionClient::Init() {
  blink::mojom::PresentationConnectionPtrInfo renderer_connection;
  connection_binding_.Bind(mojo::MakeRequest(&renderer_connection));
  auto connection_request = mojo::MakeRequest(&connection_);
  connection_->DidChangeState(
      blink::mojom::PresentationConnectionState::CONNECTED);
  return mojom::RoutePresentationConnection::New(std::move(renderer_connection),
                                                 std::move(connection_request));
}

void CastSessionClient::SendMessageToClient(
    blink::mojom::PresentationConnectionMessagePtr message) {
  connection_->OnMessage(std::move(message));
}

void CastSessionClient::SendMediaStatusToClient(
    const base::Value& media_status,
    base::Optional<int> request_id) {
  // Look up if there is a pending request from this client associated with this
  // message. If so, send the media status message as a response by setting the
  // sequence number.
  base::Optional<int> sequence_number;
  if (request_id) {
    auto it = pending_media_requests_.find(*request_id);
    if (it != pending_media_requests_.end()) {
      DVLOG(2) << "Found matching request id: " << *request_id << " -> "
               << it->second;
      sequence_number = it->second;
      pending_media_requests_.erase(it);
    }
  }

  SendMessageToClient(
      CreateV2Message(client_id_, media_status, sequence_number));
}

void CastSessionClient::OnMessage(
    blink::mojom::PresentationConnectionMessagePtr message) {
  if (!message->is_message())
    return;

  data_decoder_->ParseJson(
      message->get_message(),
      base::BindRepeating(&CastSessionClient::HandleParsedClientMessage,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ReportClientMessageParseError,
                          activity_->route().media_route_id()));
}

void CastSessionClient::DidClose(
    blink::mojom::PresentationConnectionCloseReason reason) {
  // TODO(https://crbug.com/809249): Implement close connection with this
  // method once we make sure Blink calls this on navigation and on
  // PresentationConnection::close().
}
bool CastSessionClient::MatchesAutoJoinPolicy(url::Origin origin,
                                              int tab_id) const {
  switch (auto_join_policy_) {
    case AutoJoinPolicy::kTabAndOriginScoped:
      return origin == origin_ && tab_id == tab_id_;
    case AutoJoinPolicy::kOriginScoped:
      return origin == origin_;
    default:
      return false;
  }
}

void CastSessionClient::HandleParsedClientMessage(
    std::unique_ptr<base::Value> message) {
  std::unique_ptr<CastInternalMessage> cast_message =
      CastInternalMessage::From(std::move(*message));
  if (!cast_message) {
    ReportClientMessageParseError(activity_->route().media_route_id(),
                                  "Not a Cast message");
    DLOG(ERROR) << "Received non-Cast message from client";
    return;
  }

  if (cast_message->client_id != client_id_) {
    DLOG(ERROR) << "Client ID mismatch: expected: " << client_id_
                << ", got: " << cast_message->client_id;
    return;
  }

  if (cast_message->has_session_id() &&
      cast_message->session_id() != activity_->session_id()) {
    DLOG(ERROR) << "Session ID mismatch: expected: "
                << activity_->session_id().value_or("<missing>")
                << ", got: " << cast_message->session_id();
    return;
  }

  switch (cast_message->type) {
    case CastInternalMessage::Type::kAppMessage:
      // Send an ACK message back to SDK client to indicate it is handled.
      if (activity_->SendAppMessageToReceiver(*cast_message) ==
          cast_channel::Result::kOk) {
        DCHECK(cast_message->sequence_number);
        SendMessageToClient(CreateAppMessageAck(
            cast_message->client_id, *cast_message->sequence_number));
      }
      break;

    case CastInternalMessage::Type::kV2Message:
      HandleV2ProtocolMessage(*cast_message);
      break;

    case CastInternalMessage::Type::kLeaveSession:
      SendMessageToClient(CreateLeaveSessionAckMessage(
          client_id_, cast_message->sequence_number));
      activity_->HandleLeaveSession(client_id_);
      break;

    default:
      DLOG(ERROR) << "Unhandled message type: "
                  << static_cast<int>(cast_message->type);
  }
}

void CastSessionClient::HandleV2ProtocolMessage(
    const CastInternalMessage& cast_message) {
  const std::string& type_str = cast_message.v2_message_type();
  cast_channel::V2MessageType type =
      cast_channel::V2MessageTypeFromString(type_str);
  if (cast_channel::IsMediaRequestMessageType(type)) {
    DVLOG(2) << "Got media command from client: " << type_str;
    base::Optional<int> request_id =
        activity_->SendMediaRequestToReceiver(cast_message);
    if (request_id) {
      DCHECK(cast_message.sequence_number);
      if (pending_media_requests_.size() >= kMaxPendingMediaRequests) {
        // Delete old pending requests.  Request IDs are generated sequentially,
        // so this should always delete the oldest requests.  Deleting requests
        // is O(n) in the size of the table, so we delete half the outstanding
        // requests at once so the amortized deletion cost is O(1).
        pending_media_requests_.erase(pending_media_requests_.begin(),
                                      pending_media_requests_.begin() +
                                          pending_media_requests_.size() / 2);
      }
      pending_media_requests_.emplace(*request_id,
                                      *cast_message.sequence_number);
    }
  } else if (type == cast_channel::V2MessageType::kSetVolume) {
    DVLOG(2) << "Got volume command from client";
    DCHECK(cast_message.sequence_number);
    activity_->SendSetVolumeRequestToReceiver(
        cast_message, base::BindOnce(&CastSessionClient::SendResultResponse,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     *cast_message.sequence_number));
  } else if (type == cast_channel::V2MessageType::kStop) {
    // TODO(jrw): implement STOP_SESSION.
    DVLOG(2) << "Ignoring stop-session (" << type_str << ") message";
  } else {
    DLOG(ERROR) << "Unknown v2 message type: " << type_str;
  }
}

void CastSessionClient::SendResultResponse(int sequence_number,
                                           cast_channel::Result result) {
  // TODO(jrw): Send error message on failure.
  if (result == cast_channel::Result::kOk) {
    // Send an empty message to let the client know the request succeeded.
    SendMessageToClient(
        CreateV2Message(client_id_, base::Value(), sequence_number));
  }
}

void CastSessionClient::CloseConnection() {
  if (connection_)
    connection_->DidChangeState(
        blink::mojom::PresentationConnectionState::CLOSED);

  connection_.reset();
  connection_binding_.Close();
}

void CastSessionClient::TerminateConnection() {
  if (connection_)
    connection_->DidChangeState(
        blink::mojom::PresentationConnectionState::TERMINATED);

  connection_.reset();
  connection_binding_.Close();
}

CastActivityRecord::~CastActivityRecord() {}

mojom::RoutePresentationConnectionPtr CastActivityRecord::AddClient(
    const std::string& client_id,
    const url::Origin& origin,
    int tab_id,
    AutoJoinPolicy auto_join_policy) {
  DCHECK(!base::ContainsKey(connected_clients_, client_id));
  auto client = std::make_unique<CastSessionClient>(
      client_id, origin, tab_id, auto_join_policy, data_decoder_, this);
  auto presentation_connection = client->Init();
  connected_clients_.emplace(client_id, std::move(client));
  return presentation_connection;
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
    return cast_channel::Result::kFailed;  // TODO(jrw): Send error code back to
                                           // SDK client.
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
    leaving_client_it->second->CloseConnection();
    connected_clients_.erase(leaving_client_it);
  }
}

void CastActivityRecord::SendMessageToClient(
    const std::string& client_id,
    blink::mojom::PresentationConnectionMessagePtr message) {
  auto it = connected_clients_.find(client_id);
  if (it == connected_clients_.end()) {
    DLOG(ERROR) << "Attempting to send message to nonexistent client: "
                << client_id;
    return;
  }
  it->second->SendMessageToClient(std::move(message));
}

void CastActivityRecord::ClosePresentationConnections() {
  for (auto& client : connected_clients_)
    client.second->CloseConnection();
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

CastActivityManager::CastActivityManager(
    MediaSinkServiceBase* media_sink_service,
    CastSessionTracker* session_tracker,
    cast_channel::CastMessageHandler* message_handler,
    mojom::MediaRouter* media_router,
    std::unique_ptr<DataDecoder> data_decoder,
    const std::string& hash_token)
    : media_sink_service_(media_sink_service),
      session_tracker_(session_tracker),
      message_handler_(message_handler),
      media_router_(media_router),
      data_decoder_(std::move(data_decoder)),
      hash_token_(hash_token),
      weak_ptr_factory_(this) {
  DCHECK(media_sink_service_);
  DCHECK(message_handler_);
  DCHECK(media_router_);
  DCHECK(session_tracker_);
  message_handler_->AddObserver(this);
  for (const auto& sink_id_session : session_tracker_->GetSessions()) {
    const MediaSinkInternal* sink =
        media_sink_service_->GetSinkById(sink_id_session.first);
    if (!sink)
      break;
    AddNonLocalActivityRecord(*sink, *sink_id_session.second);
  }
  session_tracker_->AddObserver(this);
}

CastActivityManager::~CastActivityManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  message_handler_->RemoveObserver(this);
  session_tracker_->RemoveObserver(this);
}

void CastActivityManager::AddRouteQuery(const MediaSource::Id& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  route_queries_.insert(source);
  std::vector<MediaRoute> routes = GetRoutes();
  if (!routes.empty())
    NotifyOnRoutesUpdated(source, routes);
}

void CastActivityManager::RemoveRouteQuery(const MediaSource::Id& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  route_queries_.erase(source);
}

void CastActivityManager::LaunchSession(
    const CastMediaSource& cast_source,
    const MediaSinkInternal& sink,
    const std::string& presentation_id,
    const url::Origin& origin,
    int tab_id,
    bool incognito,
    mojom::MediaRouteProvider::CreateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the sink is already associated with a route, then it will be removed
  // when the receiver sends an updated RECEIVER_STATUS message.
  MediaSource source(cast_source.source_id());
  const MediaSink::Id& sink_id = sink.sink().id();
  MediaRoute::Id route_id =
      MediaRoute::GetMediaRouteId(presentation_id, sink_id, source);
  MediaRoute route(route_id, source, sink_id, /* description */ std::string(),
                   /* is_local */ true, /* for_display */ true);
  route.set_incognito(incognito);
  DoLaunchSessionParams params(route, cast_source, sink, origin, tab_id,
                               std::move(callback));
  // If there is currently a session on the sink, it must be terminated before
  // the new session can be launched.
  auto it = std::find_if(
      activities_.begin(), activities_.end(), [&sink_id](const auto& activity) {
        return activity.second->route().media_sink_id() == sink_id;
      });
  if (it == activities_.end()) {
    DoLaunchSession(std::move(params));
  } else {
    const MediaRoute::Id& existing_route_id =
        it->second->route().media_route_id();
    TerminateSession(
        existing_route_id,
        base::BindOnce(
            &CastActivityManager::LaunchSessionAfterTerminatingExisting,
            GetWeakPtr(), existing_route_id, std::move(params)));
  }
}

void CastActivityManager::DoLaunchSession(DoLaunchSessionParams params) {
  const MediaRoute& route = params.route;
  const CastMediaSource& cast_source = params.cast_source;
  const MediaRoute::Id& route_id = route.media_route_id();
  const MediaSinkInternal& sink = params.sink;
  // TODO(crbug.com/904995): In the case of multiple app IDs (e.g. mirroring),
  // we need to choose an appropriate app ID to launch based on capabilities.
  std::string app_id = cast_source.GetAppIds()[0];

  DVLOG(2) << "Launching session with route ID = " << route_id
           << ", source ID = " << cast_source.source_id()
           << ", sink ID = " << sink.sink().id() << ", app ID = " << app_id
           << ", origin = " << params.origin << ", tab ID = " << params.tab_id;

  std::unique_ptr<CastActivityRecord> activity(new CastActivityRecord(
      route, app_id, media_sink_service_, message_handler_, session_tracker_,
      data_decoder_.get(), this));
  auto* activity_ptr = activity.get();
  activities_.emplace(route_id, std::move(activity));
  NotifyAllOnRoutesUpdated();
  base::TimeDelta launch_timeout = cast_source.launch_timeout();
  message_handler_->LaunchSession(
      sink.cast_data().cast_channel_id, app_id, launch_timeout,
      base::BindOnce(&CastActivityManager::HandleLaunchSessionResponse,
                     GetWeakPtr(), route_id, sink, cast_source));

  mojom::RoutePresentationConnectionPtr presentation_connection;
  const std::string& client_id = cast_source.client_id();
  if (!client_id.empty()) {
    presentation_connection =
        activity_ptr->AddClient(client_id, params.origin, params.tab_id,
                                cast_source.auto_join_policy());
    activity_ptr->SendMessageToClient(
        client_id,
        CreateReceiverActionCastMessage(client_id, sink, hash_token_));
  }

  std::move(params.callback)
      .Run(route, std::move(presentation_connection),
           /* error_text */ base::nullopt, RouteRequestResult::ResultCode::OK);
}

void CastActivityManager::LaunchSessionAfterTerminatingExisting(
    const MediaRoute::Id& existing_route_id,
    DoLaunchSessionParams params,
    const base::Optional<std::string>& error_string,
    RouteRequestResult::ResultCode result) {
  // NOTE(mfoltz): I don't recall if an explicit STOP request is required by
  // Cast V2 before launching a new session.  I think in the Javascript MRP
  // session termination is a fire and forget operation.  In which case we could
  // rely on RECEIVER_STATUS to clean up the state from the just-removed
  // session, versus having to stop then wait for a response.
  DLOG_IF(ERROR, error_string)
      << "Failed to terminate existing session before launching new "
      << "session! New session may not operate correctly. Error: "
      << *error_string;
  activities_.erase(existing_route_id);
  DoLaunchSession(std::move(params));
}

void CastActivityManager::RemoveActivity(
    ActivityMap::iterator activity_it,
    blink::mojom::PresentationConnectionState state,
    bool notify) {
  DCHECK(state == blink::mojom::PresentationConnectionState::CLOSED ||
         state == blink::mojom::PresentationConnectionState::TERMINATED);
  if (state == blink::mojom::PresentationConnectionState::CLOSED)
    activity_it->second->ClosePresentationConnections();
  else
    activity_it->second->TerminatePresentationConnections();

  activities_.erase(activity_it);
  if (notify)
    NotifyAllOnRoutesUpdated();
}

void CastActivityManager::TerminateSession(
    const MediaRoute::Id& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end()) {
    std::move(callback).Run("Activity not found",
                            RouteRequestResult::ROUTE_NOT_FOUND);
    return;
  }

  DVLOG(2) << "Terminating session with route ID: " << route_id;

  const auto& activity = activity_it->second;
  const auto& session_id = activity->session_id();
  const MediaRoute& route = activity->route();

  // There is no session associated with the route, e.g. the launch request is
  // still pending.
  if (!session_id) {
    DVLOG(2) << "Terminated route has no session ID.";
    RemoveActivity(activity_it);
    std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
    return;
  }

  const MediaSinkInternal* sink = media_sink_service_->GetSinkByRoute(route);
  CHECK(sink);

  for (auto& client : activity->connected_clients()) {
    client.second->SendMessageToClient(
        CreateReceiverActionStopMessage(client.first, *sink, hash_token_));
  }

  activity->SendStopSessionMessageToReceiver(
      base::nullopt,  // TODO(jrw): Get the real client ID.
      std::move(callback));
}

CastActivityManager::ActivityMap::iterator
CastActivityManager::FindActivityByChannelId(int channel_id) {
  return std::find_if(
      activities_.begin(), activities_.end(), [channel_id, this](auto& entry) {
        const MediaRoute& route = entry.second->route();
        const MediaSinkInternal* sink =
            media_sink_service_->GetSinkByRoute(route);
        return sink && sink->cast_data().cast_channel_id == channel_id;
      });
}

CastActivityManager::ActivityMap::iterator
CastActivityManager::FindActivityBySink(const MediaSinkInternal& sink) {
  const MediaSink::Id& sink_id = sink.sink().id();
  return std::find_if(
      activities_.begin(), activities_.end(), [&sink_id](const auto& activity) {
        return activity.second->route().media_sink_id() == sink_id;
      });
}

void CastActivityManager::OnAppMessage(
    int channel_id,
    const cast_channel::CastMessage& message) {
  // Note: app messages are received only after session is created.
  DVLOG(2) << "Received app message on cast channel " << channel_id;
  auto it = FindActivityByChannelId(channel_id);
  if (it == activities_.end()) {
    DVLOG(2) << "No activity associated with channel!";
    return;
  }

  CastActivityRecord* activity = it->second.get();
  const auto& session_id = activity->session_id();
  if (!session_id) {
    DVLOG(2) << "No session associated with activity!";
    return;
  }

  if (message.destination_id() == "*") {
    for (const auto& client : activity->connected_clients()) {
      activity->SendMessageToClient(
          client.first, CreateAppMessage(*session_id, client.first, message));
    }
  } else {
    const std::string& client_id = message.destination_id();
    activity->SendMessageToClient(
        client_id, CreateAppMessage(*session_id, client_id, message));
  }
}

void CastActivityManager::OnSessionAddedOrUpdated(const MediaSinkInternal& sink,
                                                  const CastSession& session) {
  auto activity_it = FindActivityByChannelId(sink.cast_data().cast_channel_id);

  // If |activity| is null, we have discovered a non-local activity.
  if (activity_it == activities_.end()) {
    AddNonLocalActivityRecord(sink, session);
    NotifyAllOnRoutesUpdated();
    return;
  }

  CastActivityRecord* activity = activity_it->second.get();
  DCHECK(activity->route().media_sink_id() == sink.sink().id());

  DVLOG(2) << "Receiver status: update/replace activity: "
           << activity->route().media_route_id();
  const auto& existing_session_id = activity->session_id();

  // This condition seems to always be true in practice, but if it's not, we
  // still try to handle them gracefully below.
  //
  // TODO(jrw): Replace DCHECK with an UMA metric.
  DCHECK(existing_session_id);

  // If |existing_session_id| is empty, then most likely it's due to a pending
  // launch. Check the app ID to see if the existing activity should be updated
  // or replaced.  Otherwise, check the session ID to see if the existing
  // activity should be updated or replaced.
  if (existing_session_id ? existing_session_id == session.session_id()
                          : activity->app_id() == session.app_id()) {
    activity->SetOrUpdateSession(session, sink, hash_token_);
  } else {
    // NOTE(jrw): This happens if a receiver switches to a new session (or app),
    // causing the activity associated with the old session to be considered
    // remote.  This scenario is tested in the unit tests, but it's unclear
    // whether it even happens in practice; I haven't been able to trigger it.
    //
    // TODO(jrw): Try to come up with a test to exercise this code.
    RemoveActivity(activity_it,
                   blink::mojom::PresentationConnectionState::TERMINATED,
                   /* notify */ false);
    AddNonLocalActivityRecord(sink, session);
  }
  NotifyAllOnRoutesUpdated();
}

void CastActivityManager::OnSessionRemoved(const MediaSinkInternal& sink) {
  auto it = FindActivityBySink(sink);
  if (it != activities_.end())
    RemoveActivity(it);
}

void CastActivityManager::OnMediaStatusUpdated(const MediaSinkInternal& sink,
                                               const base::Value& media_status,
                                               base::Optional<int> request_id) {
  auto it = FindActivityBySink(sink);
  if (it != activities_.end()) {
    for (auto& client : it->second->connected_clients())
      client.second->SendMediaStatusToClient(media_status, request_id);
  }
}

void CastActivityManager::AddNonLocalActivityRecord(
    const MediaSinkInternal& sink,
    const CastSession& session) {
  const MediaSink::Id& sink_id = sink.sink().id();

  // We derive the MediaSource from a session using the app ID.
  const std::string& app_id = session.app_id();
  std::unique_ptr<CastMediaSource> cast_source =
      CastMediaSource::FromAppId(app_id);
  MediaSource source(cast_source->source_id());

  // The session ID is used instead of presentation ID in determining the
  // route ID.
  MediaRoute::Id route_id =
      MediaRoute::GetMediaRouteId(session.session_id(), sink_id, source);
  DVLOG(2) << "Adding non-local route " << route_id
           << " with sink ID = " << sink_id
           << ", session ID = " << session.session_id()
           << ", app ID = " << app_id;
  // Route description is set in SetOrUpdateSession().
  MediaRoute route(route_id, source, sink_id, /* description */ std::string(),
                   /* is_local */ false, /* for_display */ true);

  std::unique_ptr<CastActivityRecord> record(new CastActivityRecord(
      route, app_id, media_sink_service_, message_handler_, session_tracker_,
      data_decoder_.get(), this));
  record->SetOrUpdateSession(session, sink, hash_token_);
  activities_.emplace(route_id, std::move(record));
}

const MediaRoute* CastActivityManager::GetRoute(
    const MediaRoute::Id& route_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = activities_.find(route_id);
  return it != activities_.end() ? &(it->second->route()) : nullptr;
}

std::vector<MediaRoute> CastActivityManager::GetRoutes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<MediaRoute> routes;
  for (const auto& activity : activities_)
    routes.push_back(activity.second->route());

  return routes;
}

void CastActivityManager::NotifyAllOnRoutesUpdated() {
  std::vector<MediaRoute> routes = GetRoutes();
  for (const auto& source_id : route_queries_)
    NotifyOnRoutesUpdated(source_id, routes);
}

void CastActivityManager::NotifyOnRoutesUpdated(
    const MediaSource::Id& source_id,
    const std::vector<MediaRoute>& routes) {
  // Note: joinable_route_ids is empty as we are deprecating the join feature
  // in the Harmony UI.
  media_router_->OnRoutesUpdated(MediaRouteProviderId::CAST, routes, source_id,
                                 std::vector<MediaRoute::Id>());
}

void CastActivityManager::HandleLaunchSessionResponse(
    const MediaRoute::Id& route_id,
    const MediaSinkInternal& sink,
    const CastMediaSource& cast_source,
    cast_channel::LaunchSessionResponse response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end()) {
    DVLOG(2) << "Route no longer exists";
    return;
  }

  if (response.result != cast_channel::LaunchSessionResponse::Result::kOk) {
    DLOG(ERROR) << "Failed to launch session for " << route_id;
    RemoveActivity(activity_it);
    SendFailedToCastIssue(sink.sink().id(), route_id);
    return;
  }

  auto session = CastSession::From(sink, *response.receiver_status);
  if (!session) {
    DLOG(ERROR) << "Unable to get session from launch response";
    RemoveActivity(activity_it);
    SendFailedToCastIssue(sink.sink().id(), route_id);
    return;
  }

  const std::string& client_id = cast_source.client_id();
  if (!client_id.empty()) {
    DVLOG(2) << "Sending new_session message for route " << route_id
             << ", client_id: " << client_id;
    activity_it->second->SendMessageToClient(
        client_id,
        CreateNewSessionMessage(*session, client_id, sink, hash_token_));

    // TODO(jrw): Query media status.
    message_handler_->EnsureConnection(sink.cast_data().cast_channel_id,
                                       client_id, session->transport_id());
  }

  activity_it->second->SetOrUpdateSession(*session, sink, hash_token_);
  NotifyAllOnRoutesUpdated();
}

void CastActivityManager::HandleStopSessionResponse(
    const MediaRoute::Id& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback,
    cast_channel::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__ << ": " << route_id;

  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end()) {
    // The activity could've been removed via RECEIVER_STATUS message.
    std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
    return;
  }

  if (result == cast_channel::Result::kOk) {
    RemoveActivity(activity_it);
    std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
  } else {
    std::move(callback).Run("Failed to terminate route",
                            RouteRequestResult::UNKNOWN_ERROR);
  }
}

void CastActivityManager::SendFailedToCastIssue(
    const MediaSink::Id& sink_id,
    const MediaRoute::Id& route_id) {
  // TODO(imcheng): i18n-ize the title string.
  IssueInfo info("Failed to cast. Please try again.",
                 IssueInfo::Action::DISMISS, IssueInfo::Severity::WARNING);
  info.sink_id = sink_id;
  info.route_id = route_id;
  media_router_->OnIssue(info);
}

base::WeakPtr<CastActivityManager> CastActivityManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

CastActivityManager::DoLaunchSessionParams::DoLaunchSessionParams(
    const MediaRoute& route,
    const CastMediaSource& cast_source,
    const MediaSinkInternal& sink,
    const url::Origin& origin,
    int tab_id,
    mojom::MediaRouteProvider::CreateRouteCallback callback)
    : route(route),
      cast_source(cast_source),
      sink(sink),
      origin(origin),
      tab_id(tab_id),
      callback(std::move(callback)) {}

CastActivityManager::DoLaunchSessionParams::DoLaunchSessionParams(
    DoLaunchSessionParams&& other) noexcept = default;

CastActivityManager::DoLaunchSessionParams::~DoLaunchSessionParams() = default;

}  // namespace media_router
