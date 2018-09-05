// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_SCREEN_PUBLISHER_H_
#define API_PUBLIC_SCREEN_PUBLISHER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/macros.h"

namespace openscreen {

// Used to report an error from a ScreenListener implementation.
struct ScreenPublisherError {
  // TODO: Add additional error types, as implementations progress.
  enum class Code {
    kNone = 0,
  };

  ScreenPublisherError();
  ScreenPublisherError(Code error, const std::string& message);
  ScreenPublisherError(const ScreenPublisherError& other);
  ~ScreenPublisherError();

  ScreenPublisherError& operator=(const ScreenPublisherError& other);

  Code error;
  std::string message;
};

class ScreenPublisher {
 public:
  enum class State {
    kStopped = 0,
    kStarting,
    kRunning,
    kStopping,
    kSuspended,
  };

  // Microseconds after the epoch.
  // TODO: Replace with a base::Time object.
  typedef uint64_t timestamp_t;

  struct Metrics {
    Metrics();
    ~Metrics();

    // The range of time over which the metrics were collected; end_timestamp >
    // start_timestamp
    timestamp_t start_timestamp = 0;
    timestamp_t end_timestamp = 0;

    // The number of packets and bytes sent since the service started.
    uint64_t num_packets_sent = 0;
    uint64_t num_bytes_sent = 0;

    // The number of packets and bytes received since the service started.
    uint64_t num_packets_received = 0;
    uint64_t num_bytes_received = 0;
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the state becomes kRunning.
    virtual void OnStarted() = 0;
    // Called when the state becomes kStopped.
    virtual void OnStopped() = 0;
    // Called when the state becomes kSuspended.
    virtual void OnSuspended() = 0;

    // Reports an error.
    virtual void OnError(ScreenPublisherError) = 0;

    // Reports metrics.
    virtual void OnMetrics(Metrics) = 0;
  };

  struct Config {
    Config();
    ~Config();

    // The human readable friendly name of the screen being published in UTF-8.
    std::string friendly_name;

    // The port where openscreen connections are accepted.
    // Normally this should not be set, and must be identical to the port
    // configured in the ScreenConnectionServer.
    uint16_t connection_server_port = 0;

    // A list of network interface names that the publisher should use.
    // By default, all enabled Ethernet and WiFi interfaces are used.
    // This configuration must be identical to the interfaces configured
    // in the ScreenConnectionServer.
    // TODO(btolsch): Can this be an index list on all platforms?
    std::vector<std::string> network_interface_names;
  };

  virtual ~ScreenPublisher();

  // Starts publishing this screen using the config object.
  // Returns true if state() == kStopped and the service will be started, false
  // otherwise.
  virtual bool Start() = 0;

  // Starts publishing this screen, but then immediately suspends the publisher.
  // No announcements will be sent until Resume() is called.
  // Returns true if state() == kStopped and the service will be started, false
  // otherwise.
  virtual bool StartAndSuspend() = 0;

  // Stops publishing this screen.
  // Returns true if state() != (kStopped|kStopping).
  virtual bool Stop() = 0;

  // Suspends publishing, for example, if the screen is in a power saving mode.
  // Returns true if state() == (kRunning|kStarting), meaning the suspension
  // will take effect.
  virtual bool Suspend() = 0;

  // Resumes publishing.  Returns true if state() == kSuspended.
  virtual bool Resume() = 0;

  // Sets the friendly name that is published.  This may also trigger a receiver
  // status message to be sent with the new friendly name.
  virtual void UpdateFriendlyName(const std::string& friendly_name) = 0;

  // Returns the current state of the publisher.
  State state() const { return state_; }

  // Returns the last error reported by this publisher.
  ScreenPublisherError last_error() const { return last_error_; }

 protected:
  explicit ScreenPublisher(Observer* observer);

  State state_ = State::kStopped;
  ScreenPublisherError last_error_;
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(ScreenPublisher);
};

}  // namespace openscreen

#endif  // API_PUBLIC_SCREEN_PUBLISHER_H_
