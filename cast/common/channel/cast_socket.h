// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_CAST_SOCKET_H_
#define CAST_COMMON_CHANNEL_CAST_SOCKET_H_

#include <array>
#include <memory>
#include <vector>

#include "cast/common/channel/proto/cast_channel.pb.h"
#include "platform/api/tls_connection.h"

namespace openscreen {
namespace cast {

// Represents a simple message-oriented socket for communicating with the Cast
// V2 protocol.  It isn't thread-safe, so it should only be used on the same
// TaskRunner thread as its TlsConnection.
class CastSocket : public TlsConnection::Client {
 public:
  class Client {
   public:
    virtual ~Client() = default;

    // Called when a terminal error on |socket| has occurred.
    virtual void OnError(CastSocket* socket, Error error) = 0;

    virtual void OnMessage(CastSocket* socket,
                           ::cast::channel::CastMessage message) = 0;
  };

  CastSocket(std::unique_ptr<TlsConnection> connection, Client* client);
  ~CastSocket();

  // Sends |message| immediately unless the underlying TLS connection is
  // write-blocked, in which case |message| will be queued.  An error will be
  // returned if |message| cannot be serialized for any reason, even while
  // write-blocked.
  [[nodiscard]] Error SendMessage(const ::cast::channel::CastMessage& message);

  void SetClient(Client* client);

  std::array<uint8_t, 2> GetSanitizedIpAddress();

  int socket_id() const { return socket_id_; }

  void set_audio_only(bool audio_only) { audio_only_ = audio_only; }
  bool audio_only() const { return audio_only_; }

  // TlsConnection::Client overrides.
  void OnError(TlsConnection* connection, Error error) override;
  void OnRead(TlsConnection* connection, std::vector<uint8_t> block) override;

 private:
  enum class State : bool {
    kOpen = true,
    kError = false,
  };

  static int g_next_socket_id_;

  const std::unique_ptr<TlsConnection> connection_;
  Client* client_;  // May never be null.
  const int socket_id_;
  bool audio_only_ = false;
  std::vector<uint8_t> read_buffer_;
  State state_ = State::kOpen;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_CHANNEL_CAST_SOCKET_H_
