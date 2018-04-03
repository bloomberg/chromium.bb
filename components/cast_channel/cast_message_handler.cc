// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_message_handler.h"

#include <tuple>

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "components/cast_channel/cast_socket_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace cast_channel {

namespace {

constexpr net::NetworkTrafficAnnotationTag kMessageTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cast_message_handler", R"(
        semantics {
          sender: "Cast Message Handler"
          description:
            "A Cast protocol or application-level message sent to a Cast "
            "device."
          trigger:
            "Triggered by user gesture from using Cast functionality, or "
            "a webpage using the Presentation API, or "
            "Cast device discovery internal logic."
          data:
            "A serialized Cast protocol or application-level protobuf message. "
            "A non-exhaustive list of Cast protocol messages:\n"
            "- Virtual connection requests,\n"
            "- App availability / media status / receiver status requests,\n"
            "- Launch / stop Cast session requests,\n"
            "- Media commands, such as play/pause.\n"
            "Application-level messages may contain data specific to the Cast "
            "application."
          destination: OTHER
          destination_other:
            "Data will be sent to a Cast device in local network."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be disabled, but it would not be sent if user "
            "does not connect a Cast device to the local network."
          policy_exception_justification: "Not implemented."
        })");

}  // namespace

GetAppAvailabilityRequest::GetAppAvailabilityRequest(
    int channel_id,
    const std::string& app_id,
    GetAppAvailabilityCallback callback,
    const base::TickClock* clock)
    : channel_id(channel_id),
      app_id(app_id),
      callback(std::move(callback)),
      timeout_timer(clock) {}

GetAppAvailabilityRequest::~GetAppAvailabilityRequest() = default;

VirtualConnection::VirtualConnection(int channel_id,
                                     const std::string& source_id,
                                     const std::string& destination_id)
    : channel_id(channel_id),
      source_id(source_id),
      destination_id(destination_id) {}
VirtualConnection::~VirtualConnection() = default;

bool VirtualConnection::operator<(const VirtualConnection& other) const {
  return std::tie(channel_id, source_id, destination_id) <
         std::tie(other.channel_id, other.source_id, other.destination_id);
}

CastMessageHandler::CastMessageHandler(CastSocketService* socket_service,
                                       const std::string& user_agent,
                                       const std::string& browser_version)
    : sender_id_(base::StringPrintf("sender-%d", base::RandInt(0, 1000000))),
      user_agent_(user_agent),
      browser_version_(browser_version),
      socket_service_(socket_service),
      clock_(base::DefaultTickClock::GetInstance()),
      weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  socket_service_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastSocketService::AddObserver,
                                base::Unretained(socket_service_),
                                base::Unretained(this)));
}

CastMessageHandler::~CastMessageHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  socket_service_->RemoveObserver(this);
}

void CastMessageHandler::RequestAppAvailability(
    CastSocket* socket,
    const std::string& app_id,
    GetAppAvailabilityCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int channel_id = socket->id();
  auto pending_it = std::find_if(
      pending_app_availability_requests_.begin(),
      pending_app_availability_requests_.end(),
      [&channel_id, &app_id](
          const std::pair<int, std::unique_ptr<GetAppAvailabilityRequest>>&
              entry) {
        const auto& request = entry.second;
        return request->channel_id == channel_id && app_id == request->app_id;
      });
  if (pending_it != pending_app_availability_requests_.end())
    return;

  int request_id = NextRequestId();

  DVLOG(2) << __func__ << ", socket_id: " << socket->id()
           << ", app_id: " << app_id << ", request_id: " << request_id;
  CastMessage message =
      CreateGetAppAvailabilityRequest(sender_id_, request_id, app_id);

  auto request = std::make_unique<GetAppAvailabilityRequest>(
      channel_id, app_id, std::move(callback), clock_);
  request->timeout_timer.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(5),
      base::Bind(&CastMessageHandler::AppAvailabilityTimedOut,
                 base::Unretained(this), request_id));
  pending_app_availability_requests_.emplace(request_id, std::move(request));

  SendCastMessage(socket, message);
}

void CastMessageHandler::AppAvailabilityTimedOut(int request_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << __func__ << ", request_id: " << request_id;
  auto it = pending_app_availability_requests_.find(request_id);
  if (it == pending_app_availability_requests_.end())
    return;

  std::move(it->second->callback)
      .Run(it->second->app_id, GetAppAvailabilityResult::kUnknown);
  pending_app_availability_requests_.erase(it);
}

void CastMessageHandler::OnError(const CastSocket& socket,
                                 ChannelError error_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::EraseIf(virtual_connections_,
                [&socket](const VirtualConnection& connection) {
                  return connection.channel_id == socket.id();
                });

  auto it = pending_app_availability_requests_.begin();
  while (it != pending_app_availability_requests_.end()) {
    if (it->second->channel_id == socket.id()) {
      std::move(it->second->callback)
          .Run(it->second->app_id, GetAppAvailabilityResult::kUnknown);
      it = pending_app_availability_requests_.erase(it);
    } else {
      ++it;
    }
  }
}

void CastMessageHandler::OnMessage(const CastSocket& socket,
                                   const CastMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/698940): Handle other messages.
  DVLOG(2) << __func__ << ", socket_id: " << socket.id()
           << ", message: " << CastMessageToString(message);
  if (!IsReceiverMessage(message) || message.destination_id() != sender_id_)
    return;

  std::unique_ptr<base::DictionaryValue> payload =
      GetDictionaryFromCastMessage(message);
  if (!payload)
    return;

  int request_id = 0;
  if (!GetRequestIdFromResponse(*payload, &request_id))
    return;

  auto it = pending_app_availability_requests_.find(request_id);
  if (it != pending_app_availability_requests_.end()) {
    GetAppAvailabilityResult result =
        GetAppAvailabilityResultFromResponse(*payload, it->second->app_id);
    std::move(it->second->callback).Run(it->second->app_id, result);
    pending_app_availability_requests_.erase(it);
  }
}

void CastMessageHandler::SendCastMessage(CastSocket* socket,
                                         const CastMessage& message) {
  // A virtual connection must be opened to the receiver before other messages
  // can be sent.
  VirtualConnection connection(socket->id(), message.source_id(),
                               message.destination_id());
  if (virtual_connections_.find(connection) == virtual_connections_.end()) {
    DVLOG(1) << "Creating VC for channel: " << connection.channel_id
             << ", source: " << connection.source_id
             << ", dest: " << connection.destination_id;
    CastMessage virtual_connection_request = CreateVirtualConnectionRequest(
        connection.source_id, connection.destination_id,
        connection.destination_id == kPlatformReceiverId
            ? VirtualConnectionType::kStrong
            : VirtualConnectionType::kInvisible,
        user_agent_, browser_version_);
    socket->transport()->SendMessage(
        virtual_connection_request,
        base::Bind(&CastMessageHandler::OnMessageSent,
                   weak_ptr_factory_.GetWeakPtr()),
        kMessageTrafficAnnotation);

    // We assume the virtual connection request will succeed; otherwise this
    // will eventually self-correct.
    virtual_connections_.insert(connection);
  }
  socket->transport()->SendMessage(
      message,
      base::Bind(&CastMessageHandler::OnMessageSent,
                 weak_ptr_factory_.GetWeakPtr()),
      kMessageTrafficAnnotation);
}

void CastMessageHandler::OnMessageSent(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG_IF(2, result < 0) << "SendMessage failed with code: " << result;
}

}  // namespace cast_channel
