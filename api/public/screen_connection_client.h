// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_SCREEN_CONNECTION_CLIENT_H_
#define API_PUBLIC_SCREEN_CONNECTION_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "base/time.h"
#include "api/public/screen_connection.h"

namespace openscreen {

// Used to report an error from a ScreenConnectionClient implementation.
// NOTE: May be simpler to have a combined enum/struct for client and server.
struct ScreenConnectionClientError {
 public:
  // TODO(mfoltz): Add additional error types, as implementations progress.
  enum class Code {
    kNone = 0,
  };

  ScreenConnectionClientError();
  ScreenConnectionClientError(Code error, const std::string& message);
  ScreenConnectionClientError(const ScreenConnectionClientError& other);
  ~ScreenConnectionClientError();

  ScreenConnectionClientError& operator=(
      const ScreenConnectionClientError& other);

  Code error = Code::kNone;
  std::string message;
};

class ScreenConnectionClient {
 public:
  enum class State {
    kStopped = 0,
    kRunning
  };

  // For any embedder-specific configuration of the ScreenConnectionClient.
  struct Config {
    Config();
    ~Config();
  };

  // Holds a set of metrics, captured over a specific range of time, about the
  // behavior of a ScreenConnectionClient instance.
  struct Metrics {
    Metrics();
    ~Metrics();

    // The range of time over which the metrics were collected; end_timestamp >
    // start_timestamp
    timestamp_t start_timestamp = 0;
    timestamp_t end_timestamp = 0;

    // The number of packets and bytes sent over the timestamp range.
    uint64_t num_packets_sent = 0;
    uint64_t num_bytes_sent = 0;

    // The number of packets and bytes received over the timestamp range.
    uint64_t num_packets_received = 0;
    uint64_t num_bytes_received = 0;

    // The maximum number of connections over the timestamp range.  The
    // latter two fields break this down by connections to ipv4 and ipv6
    // endpoints.
    size_t num_connections = 0;
    size_t num_ipv4_connections = 0;
    size_t num_ipv6_connections = 0;
  };

  class Observer : public ScreenConnectionObserverBase {
   public:
    virtual ~Observer() = default;

    // Reports an error.
    virtual void OnError(ScreenConnectionClientError) = 0;

    // Reports metrics.
    virtual void OnMetrics(Metrics) = 0;
  };

  virtual ~ScreenConnectionClient();

  // Starts the client using the config object.
  // Returns true if state() == kStopped and the service will be started,
  // false otherwise.
  virtual bool Start() = 0;

  // Stops listening and cancels any search in progress.
  // Returns true if state() != (kStopped|kStopping).
  virtual bool Stop() = 0;

  // Returns the current state of the listener.
  State state() const { return state_; }

  // Returns the last error reported by this client.
  const ScreenConnectionClientError& last_error() const { return last_error_; }

 protected:
  explicit ScreenConnectionClient(const Config& config, Observer* observer);

  Config config_;
  State state_ = State::kStopped;
  ScreenConnectionClientError last_error_;
  Observer* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ScreenConnectionClient);
};

}  // namespace openscreen

#endif  // API_PUBLIC_SCREEN_CONNECTION_CLIENT_H_
