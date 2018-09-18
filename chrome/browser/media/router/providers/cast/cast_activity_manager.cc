// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"

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
                                     DataDecoder* data_decoder,
                                     CastActivityRecord* activity)
    : client_id_(client_id),
      origin_(origin),
      tab_id_(tab_id),
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

void CastSessionClient::HandleParsedClientMessage(
    std::unique_ptr<base::Value> message) {
  std::unique_ptr<CastInternalMessage> cast_message =
      CastInternalMessage::From(std::move(*message));
  if (!cast_message) {
    ReportClientMessageParseError(activity_->route().media_route_id(),
                                  "Not a Cast message");
    return;
  }

  if (cast_message->type == CastInternalMessage::Type::kAppMessage) {
    if (cast_message->client_id != client_id_)
      return;

    // Send an ACK message back to SDK client to indicate it is handled.
    if (activity_->SendAppMessageToReceiver(*cast_message)) {
      SendMessageToClient(CreateAppMessageAck(cast_message->client_id,
                                              cast_message->sequence_number));
    }
  } else {
    DVLOG(2) << "Unhandled message type: "
             << static_cast<int>(cast_message->type);
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

CastActivityRecord::CastActivityRecord(
    const MediaRoute& route,
    MediaSinkServiceBase* media_sink_service,
    cast_channel::CastMessageHandler* message_handler,
    DataDecoder* data_decoder)
    : route_(route),
      media_sink_service_(media_sink_service),
      message_handler_(message_handler),
      data_decoder_(data_decoder) {}

CastActivityRecord::~CastActivityRecord() {}

mojom::RoutePresentationConnectionPtr CastActivityRecord::AddClient(
    const std::string& client_id,
    const url::Origin& origin,
    int tab_id) {
  DCHECK(!base::ContainsKey(connected_clients_, client_id));
  auto client = std::make_unique<CastSessionClient>(client_id, origin, tab_id,
                                                    data_decoder_, this);
  auto presentation_connection = client->Init();
  connected_clients_.emplace(client_id, std::move(client));
  return presentation_connection;
}

void CastActivityRecord::SetSession(std::unique_ptr<CastSession> session) {
  session_ = std::move(session);
  route_.set_description(CastSession::GetRouteDescription(*session_));
}

bool CastActivityRecord::SendAppMessageToReceiver(
    const CastInternalMessage& cast_message) {
  DCHECK_EQ(CastInternalMessage::Type::kAppMessage, cast_message.type);

  const MediaSink::Id& sink_id = route_.media_sink_id();
  const MediaSinkInternal* sink = media_sink_service_->GetSinkById(sink_id);
  if (!sink) {
    DVLOG(2) << "Sink not found";
    return false;
  }

  if (!session_ ||
      session_->session_id != cast_message.app_message_session_id) {
    DVLOG(2) << "Session not found: " << cast_message.app_message_session_id;
    return false;
  }

  const std::string& message_namespace = cast_message.app_message_namespace;
  if (!base::ContainsKey(session_->message_namespaces, message_namespace)) {
    DVLOG(2) << "Disallowed message namespace: " << message_namespace;
    // TODO(imcheng): Send error code back to SDK client.
    return false;
  }

  message_handler_->SendAppMessage(
      sink->cast_data().cast_channel_id,
      cast_channel::CreateCastMessage(
          message_namespace, cast_message.app_message_body,
          cast_message.client_id, session_->transport_id));
  return true;
}

void CastActivityRecord::SendMessageToClient(
    const std::string& client_id,
    blink::mojom::PresentationConnectionMessagePtr message) {
  auto it = connected_clients_.find(client_id);
  if (it == connected_clients_.end()) {
    DVLOG(2) << "Attempting to send message to nonexistent client: "
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

CastActivityManager::CastActivityManager(
    MediaSinkServiceBase* media_sink_service,
    cast_channel::CastMessageHandler* message_handler,
    mojom::MediaRouter* media_router,
    std::unique_ptr<DataDecoder> data_decoder,
    const std::string& hash_token)
    : media_sink_service_(media_sink_service),
      message_handler_(message_handler),
      media_router_(media_router),
      data_decoder_(std::move(data_decoder)),
      hash_token_(hash_token),
      weak_ptr_factory_(this) {
  DCHECK(media_sink_service_);
  DCHECK(message_handler_);
  DCHECK(media_router_);
  message_handler_->AddObserver(this);
}

CastActivityManager::~CastActivityManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  message_handler_->RemoveObserver(this);
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

  // We will preemptively add the route to the list while the session launch
  // request is pending. This way we can report back to Media Router that a
  // route is added without waiting for the response.
  // Route description will be filled in later with session data returned by
  // the Cast receiver.
  MediaRoute route(route_id, source, sink_id, /* description */ std::string(),
                   /* is_local */ true, /* for_display */ true);
  route.set_incognito(incognito);

  auto activity = std::make_unique<CastActivityRecord>(
      route, media_sink_service_, message_handler_, data_decoder_.get());
  CastActivityRecord* activity_ptr = activity.get();
  activities_.emplace(route_id, std::move(activity));
  NotifyAllOnRoutesUpdated();

  // TODO(imcheng): In the case of multiple app IDs (e.g. mirroring), we need to
  // choose an appropriate app ID to launch based on capabilities.
  std::string app_id = cast_source.GetAppIds()[0];
  base::TimeDelta launch_timeout = cast_source.launch_timeout();
  message_handler_->LaunchSession(
      sink.cast_data().cast_channel_id, app_id, launch_timeout,
      base::BindOnce(&CastActivityManager::HandleLaunchSessionResponse,
                     weak_ptr_factory_.GetWeakPtr(), route_id, sink,
                     cast_source));

  mojom::RoutePresentationConnectionPtr presentation_connection;
  const std::string& client_id = cast_source.client_id();
  if (!client_id.empty()) {
    presentation_connection =
        activity_ptr->AddClient(client_id, origin, tab_id);
    activity_ptr->SendMessageToClient(
        client_id,
        CreateReceiverActionCastMessage(client_id, sink, hash_token_));
  }

  std::move(callback).Run(route, std::move(presentation_connection),
                          /* error_text */ base::nullopt,
                          RouteRequestResult::ResultCode::OK);
}

void CastActivityManager::RemoveActivity(
    ActivityMap::iterator activity_it,
    blink::mojom::PresentationConnectionState state) {
  DCHECK(state == blink::mojom::PresentationConnectionState::CLOSED ||
         state == blink::mojom::PresentationConnectionState::TERMINATED);
  if (state == blink::mojom::PresentationConnectionState::CLOSED)
    activity_it->second->ClosePresentationConnections();
  else
    activity_it->second->TerminatePresentationConnections();

  activities_.erase(activity_it);
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

  const auto& activity = activity_it->second;
  const CastSession* session = activity->session();
  const MediaRoute& route = activity->route();

  // There is no session associated with the route, e.g. the launch request is
  // still pending.
  if (!session) {
    RemoveActivity(activity_it);
    std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
    return;
  }

  const MediaSinkInternal* sink =
      media_sink_service_->GetSinkById(route.media_sink_id());
  if (!sink) {
    RemoveActivity(activity_it);
    std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
    return;
  }

  for (auto& client : activity->connected_clients()) {
    client.second->SendMessageToClient(
        CreateReceiverActionStopMessage(client.first, *sink, hash_token_));
  }

  message_handler_->StopSession(
      sink->cast_data().cast_channel_id, session->session_id,
      base::BindOnce(&CastActivityManager::HandleStopSessionResponse,
                     weak_ptr_factory_.GetWeakPtr(), route_id,
                     std::move(callback)));
}

CastActivityManager::ActivityMap::iterator
CastActivityManager::GetActivityByChannelId(int channel_id) {
  return std::find_if(
      activities_.begin(), activities_.end(), [&channel_id, this](auto& entry) {
        const MediaRoute& route = entry.second->route();
        const MediaSinkInternal* sink =
            media_sink_service_->GetSinkById(route.media_sink_id());
        return sink && sink->cast_data().cast_channel_id == channel_id;
      });
}

void CastActivityManager::OnAppMessage(
    int channel_id,
    const cast_channel::CastMessage& message) {
  // Note: app messages are received only after session is created.
  DVLOG(2) << "Received app message on cast channel " << channel_id;
  auto it = GetActivityByChannelId(channel_id);
  if (it == activities_.end() || !it->second->session())
    return;

  CastActivityRecord* activity = it->second.get();
  const CastSession* session = activity->session();
  if (message.destination_id() == "*") {
    for (const auto& client : activity->connected_clients()) {
      activity->SendMessageToClient(
          client.first,
          CreateAppMessage(session->session_id, client.first, message));
    }
  } else {
    const std::string& client_id = message.destination_id();
    activity->SendMessageToClient(
        client_id, CreateAppMessage(session->session_id, client_id, message));
  }
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
    DVLOG(2) << "Failed to launch session for " << route_id;
    RemoveActivity(activity_it);
    SendFailedToCastIssue(sink.sink().id(), route_id);
    return;
  }

  auto session =
      CastSession::From(sink, hash_token_, *response.receiver_status);
  if (!session) {
    DVLOG(2) << "Unable to get session from launch response";
    RemoveActivity(activity_it);
    SendFailedToCastIssue(sink.sink().id(), route_id);
    return;
  }

  const std::string& client_id = cast_source.client_id();
  if (!client_id.empty()) {
    DVLOG(2) << "Sending new_session message for route " << route_id
             << ", client_id: " << client_id;
    activity_it->second->SendMessageToClient(
        client_id, CreateNewSessionMessage(*session, cast_source.client_id()));

    // TODO(imcheng): Query media status.
    message_handler_->EnsureConnection(sink.cast_data().cast_channel_id,
                                       cast_source.client_id(),
                                       session->transport_id);
  }

  activity_it->second->SetSession(std::move(session));
  NotifyAllOnRoutesUpdated();
}

void CastActivityManager::HandleStopSessionResponse(
    const MediaRoute::Id& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto activity_it = activities_.find(route_id);
  if (activity_it == activities_.end()) {
    std::move(callback).Run("Activity not found",
                            RouteRequestResult::ROUTE_NOT_FOUND);
    return;
  }

  if (success) {
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

}  // namespace media_router
