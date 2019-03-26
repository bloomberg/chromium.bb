// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PRESENTATION_PRESENTATION_CONNECTION_H_
#define API_PUBLIC_PRESENTATION_PRESENTATION_CONNECTION_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/public/message_demuxer.h"
#include "base/ip_address.h"
#include "base/macros.h"
#include "platform/api/logging.h"
#include "platform/api/time.h"
#include "third_party/abseil/src/absl/strings/string_view.h"
#include "third_party/abseil/src/absl/types/optional.h"

namespace openscreen {

class ProtocolConnection;

namespace presentation {

enum class TerminationReason {
  kReceiverTerminateCalled = 0,
  kReceiverUserTerminated,
  kControllerTerminateCalled,
  kControllerUserTerminated,
  kReceiverPresentationReplaced,
  kReceiverIdleTooLong,
  kReceiverPresentationUnloaded,
  kReceiverShuttingDown,
  kReceiverError,
};

class Connection {
 public:
  enum class CloseReason {
    kClosed = 0,
    kDiscarded,
    kError,
  };

  enum class State {
    // The library is currently attempting to connect to the presentation.
    kConnecting,
    // The connection to the presentation is open and communication is possible.
    kConnected,
    // The connection is closed or could not be opened.  No communication is
    // possible but it may be possible to reopen the connection via
    // ReconnectPresentation.
    kClosed,
    // The connection is closed and the receiver has been terminated.
    kTerminated,
  };

  // An object to receive callbacks related to a single Connection.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // State changes.
    virtual void OnConnected() = 0;

    // Explicit close by other endpoint.
    virtual void OnClosedByRemote() = 0;

    // Closed because the script connection object was discarded.
    virtual void OnDiscarded() = 0;

    // Closed because of an error.
    virtual void OnError(const absl::string_view message) = 0;

    // Terminated through a different connection.
    virtual void OnTerminatedByRemote() = 0;

    // A UTF-8 string message was received.
    virtual void OnStringMessage(const absl::string_view message) = 0;

    // A binary message was received.
    virtual void OnBinaryMessage(const std::vector<uint8_t>& data) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  enum class Role {
    kController,
    kReceiver,
  };

  struct PresentationInfo {
    std::string id;
    std::string url;
  };

  // Constructs a new connection using |delegate| for callbacks.
  Connection(const PresentationInfo& info, Role role, Delegate* delegate);
  ~Connection();

  // Returns the ID and URL of this presentation.
  const PresentationInfo& presentation_info() const { return presentation_; }

  Role role() const { return role_; }
  State state() const { return state_; }

  ProtocolConnection* get_protocol_connection() const {
    return protocol_connection_.get();
  }

  // These methods should only be called when we are connected.
  uint64_t endpoint_id() const {
    OSP_CHECK(endpoint_id_);
    return endpoint_id_.value();
  }
  uint64_t connection_id() const {
    OSP_CHECK(connection_id_);
    return connection_id_.value();
  }

  // Sends a UTF-8 string message.
  Error SendString(absl::string_view message);

  // Sends a binary message.
  Error SendBinary(std::vector<uint8_t>&& data);

  // Closes the connection.  This can be based on an explicit request from the
  // embedder or because the connection object is being discarded (page
  // navigated, object GC'd, etc.).
  Error Close(CloseReason reason);

  // Terminates the presentation associated with this connection.
  void Terminate(TerminationReason reason);

  // Called by the receiver when the OnPresentationStarted logic happens. This
  // notifies the delegate and updates our internal stream and ids.
  void OnConnected(uint64_t connection_id,
                   uint64_t endpoint_id,
                   std::unique_ptr<ProtocolConnection> stream);

  void OnClosedByError(Error cause);
  void OnClosedByRemote();
  void OnTerminatedByRemote();

  Delegate* get_delegate() { return delegate_; }

 private:
  // Helper method that handles closing down our internal state.
  // Returns whether or not the connection state changed (and thus
  // whether or not delegates should be informed).
  bool OnClosed();

  PresentationInfo presentation_;
  State state_ = State::kConnecting;
  Delegate* delegate_;
  Role role_;
  absl::optional<uint64_t> connection_id_;
  absl::optional<uint64_t> endpoint_id_;
  std::unique_ptr<ProtocolConnection> protocol_connection_;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

class ConnectionManager final : public MessageDemuxer::MessageCallback {
 public:
  explicit ConnectionManager(MessageDemuxer* demuxer);

  void AddConnection(Connection* connection);
  void RemoveConnection(Connection* connection);

  // MessasgeDemuxer::MessageCallback overrides.
  ErrorOr<size_t> OnStreamMessage(uint64_t endpoint_id,
                                  uint64_t connection_id,
                                  msgs::Type message_type,
                                  const uint8_t* buffer,
                                  size_t buffer_size,
                                  platform::Clock::time_point now) override;

  Connection* GetConnection(const std::string& presentation_id,
                            uint64_t connection_id);

 private:
  // <presentation id, connection id> -> Connection
  std::map<std::pair<std::string, uint64_t>, Connection*> connections_;

  MessageDemuxer::MessageWatch message_watch_;
  MessageDemuxer::MessageWatch close_request_watch_;
  MessageDemuxer::MessageWatch close_event_watch_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
};

}  // namespace presentation
}  // namespace openscreen

#endif  // API_PUBLIC_PRESENTATION_PRESENTATION_CONNECTION_H_
