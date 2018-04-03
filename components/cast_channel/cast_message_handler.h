// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CHANNEL_CAST_MESSAGE_HANDLER_H_
#define COMPONENTS_CAST_CHANNEL_CAST_MESSAGE_HANDLER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "components/cast_channel/cast_message_util.h"
#include "components/cast_channel/cast_socket.h"

namespace cast_channel {

class CastSocketService;

// |app_id|: ID of app the result is for.
// |result|: Availability result from the receiver.
using GetAppAvailabilityCallback =
    base::OnceCallback<void(const std::string& app_id,
                            GetAppAvailabilityResult result)>;

// Represents an app availability request to a Cast sink.
struct GetAppAvailabilityRequest {
  GetAppAvailabilityRequest(int channel_id,
                            const std::string& app_id,
                            GetAppAvailabilityCallback callback,
                            const base::TickClock* clock);
  ~GetAppAvailabilityRequest();

  // ID of channel the request is sent over.
  int channel_id;

  // App ID of the request.
  std::string app_id;

  // Callback to invoke when availability has been determined.
  GetAppAvailabilityCallback callback;

  // Timer to fail the request on timeout.
  base::OneShotTimer timeout_timer;
};

// Represents a virtual connection on a cast channel. A virtual connection is
// given by a source and destination ID pair, and must be created before
// messages can be sent. Virtual connections are managed by CastMessageHandler.
struct VirtualConnection {
  VirtualConnection(int channel_id,
                    const std::string& source_id,
                    const std::string& destination_id);
  ~VirtualConnection();

  bool operator<(const VirtualConnection& other) const;

  // ID of cast channel.
  int channel_id;

  // Source ID (e.g. sender-0).
  std::string source_id;

  // Destination ID (e.g. receiver-0).
  std::string destination_id;
};

// Handles messages that are sent between this browser instance and the Cast
// devices connected to it. This class also manages virtual connections (VCs)
// with each connected Cast device and ensures a proper VC exists before the
// message is sent. This makes the concept of VC transparent to the client.
// This class currently provides only supports requesting app availability from
// a device, but will be expanded to support additional types of messages.
// This class may be created on any sequence, but other methods (including
// destructor) must be run on the same sequence that CastSocketService runs on.
class CastMessageHandler : public CastSocket::Observer {
 public:
  explicit CastMessageHandler(CastSocketService* socket_service,
                              const std::string& user_agent,
                              const std::string& browser_version);
  ~CastMessageHandler() override;

  // Sends an app availability for |app_id| to the device given by |socket|.
  // |callback| is always invoked asynchronously, and will be invoked when a
  // response is received, or if the request timed out. No-ops if there is
  // already a pending request with the same socket and app ID.
  virtual void RequestAppAvailability(CastSocket* socket,
                                      const std::string& app_id,
                                      GetAppAvailabilityCallback callback);

  const std::string& sender_id() const { return sender_id_; }

 private:
  friend class CastMessageHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(CastMessageHandlerTest, RequestAppAvailability);
  FRIEND_TEST_ALL_PREFIXES(CastMessageHandlerTest,
                           RecreateVirtualConnectionAfterError);

  // Used internally to generate the next ID to use in a request type message.
  int NextRequestId() { return ++next_request_id_; }

  // CastSocket::Observer implementation.
  void OnError(const CastSocket& socket, ChannelError error_state) override;
  void OnMessage(const CastSocket& socket, const CastMessage& message) override;

  // Sends |message| over |socket|. This also ensures the necessary virtual
  // connection exists before sending the message.
  void SendCastMessage(CastSocket* socket, const CastMessage& message);

  // Callback for CastTransport::SendMessage.
  void OnMessageSent(int result);

  // Invokes the pending callback associated with |request_id| with kUnknown.
  void AppAvailabilityTimedOut(int request_id);

  // App availability requests pending responses, keyed by request ID.
  base::flat_map<int, std::unique_ptr<GetAppAvailabilityRequest>>
      pending_app_availability_requests_;

  // Source ID used for platform messages. The suffix is randomized to
  // distinguish it from other Cast senders on the same network.
  const std::string sender_id_;

  // User agent and browser version strings included in virtual connection
  // messages.
  const std::string user_agent_;
  const std::string browser_version_;

  int next_request_id_ = 0;

  // Set of virtual connections opened to receivers.
  base::flat_set<VirtualConnection> virtual_connections_;

  CastSocketService* const socket_service_;

  // Non-owned pointer to TickClock used for request timeouts.
  const base::TickClock* const clock_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastMessageHandler);
};

}  // namespace cast_channel

#endif  // COMPONENTS_CAST_CHANNEL_CAST_MESSAGE_HANDLER_H_
