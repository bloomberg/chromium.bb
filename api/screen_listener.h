// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_SCREEN_LISTENER_H_
#define API_SCREEN_LISTENER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/ip_address.h"

namespace openscreen {

enum class ScreenListenerState {
  kStopped = 0,
  kStarting,
  kRunning,
  kStopping,
  kSearching,
  kSuspended
};

// TODO: Add additional error types, as implementations progress.
enum class ScreenListenerError {
  kNone = 0
};

// Used to report an error from a ScreenListener implementation.
struct ScreenListenerErrorInfo {
 public:
  ScreenListenerErrorInfo();
  ScreenListenerErrorInfo(ScreenListenerError error, const std::string& message);
  ~ScreenListenerErrorInfo();

  const ScreenListenerError error;
  const std::string message;
};

// Microseconds after the epoch.
// TODO: Replace with a base::Time object.
typedef uint64_t timestamp_t;

// Holds a set of metrics, captured over a specific range of time, about the
// behavior of a ScreenListener instance.
struct ScreenListenerMetrics {
  ScreenListenerMetrics();
  ~ScreenListenerMetrics();

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

  // The maximum number of screens discovered over the timestamp range.  The
  // latter two fields break this down by screens advertising ipv4 and ipv6 endpoints.
  size_t num_screens = 0;
  size_t num_ipv4_screens = 0;
  size_t num_ipv6_screens = 0;
};

// Holds information about a screen discovered by a ScreenListener.
struct ScreenInfo {
  explicit ScreenInfo(const std::string& screen_id);
  ~ScreenInfo();

  // Identifier uniquely identifying the screen among all screens discovered by
  // this instance of library.
  std::string screen_id;

  // User visible name of the screen in UTF-8.
  std::string friendly_name;

  // The network interface that the screen was discovered on.
  std::string network_interface;

  // The network endpoint to create a new connection to the screen.
  // At least one of these two must be set.
  IPv4Endpoint ipv4_endpoint;
  IPv6Endpoint ipv6_endpoint;
};

// Observer invoked when the set of screens discovered by the library has
// changed.
class ScreenListenerObserver {
 public:
  // Called when the state becomes RUNNING.
  virtual void OnStarted() = 0;
  // Called when the state becomes STOPPED.
  virtual void OnStopped() = 0;
  // Called when the state becomes SUSPENDED.
  virtual void OnSuspended() = 0;
  // Called when the state becomes SEARCHING.
  virtual void OnSearching() = 0;

  // Notifications to changes to the listener's screen list.
  virtual void OnScreenAdded(const ScreenInfo&) = 0;
  virtual void OnScreenChanged(const ScreenInfo&) = 0;
  virtual void OnScreenRemoved(const ScreenInfo&) = 0;
  // Called if all screens are no longer available, e.g. all network interfaces
  // have been disabled.
  virtual void OnAllScreensRemoved() = 0;

  // Reports an error.
  virtual void OnError(ScreenListenerErrorInfo) = 0;

  // Reports metrics.
  virtual void OnMetrics(ScreenListenerMetrics) = 0;
};

class ScreenListener {
 public:
  virtual ~ScreenListener();

  // Starts listening for screens using the config object.
  // Returns true if state() == STOPPED and the service will be started,
  // false otherwise.
  virtual bool Start() = 0;

  // Starts the listener in SUSPENDED mode.  This could be used to enable
  // immediate search via SearchNow() in the future.
  // Returns true if state() == STOPPED and the service will be started,
  // false otherwise.
  virtual bool StartAndSuspend() = 0;

  // Stops listening and cancels any search in progress.
  // Returns true if state() != STOPPED.
  virtual bool Stop() = 0;

  // Suspends background listening. For example, the tab wanting screen
  // availability might go in the background, meaning we can suspend listening
  // to save power.
  // Returns true if state() == (RUNNING|SEARCHING|STARTING), meaning the
  // suspension will take effect.
  virtual bool Suspend() = 0;

  // Resumes listening.  Returns true if state() == SUSPENDED.
  virtual bool Resume() = 0;

  // Asks the listener to search for screens now, even if the listener is
  // is currently suspended.  If a background search is already in
  // progress, this has no effect.  Returns true if state() !=
  // (STOPPED|STARTING).
  virtual bool SearchNow() = 0;

  // Returns the current state of the listener.
  ScreenListenerState state() const { return state_; }

  // Returns the last error reported by this listener.
  const ScreenListenerErrorInfo& GetLastError() const { return last_error_; }

  // Must be called with a valid observer before the listener is started.
  void SetObserver(ScreenListenerObserver* observer) { observer_ = observer; }

  // Returns the current list of screens known to the ScreenListener.
  const std::vector<ScreenInfo>& GetScreens() const { return screens_; }

 protected:
  ScreenListener();

  ScreenListenerState state_ = ScreenListenerState::kStopped;
  ScreenListenerErrorInfo last_error_;
  ScreenListenerObserver* observer_ = nullptr;
  std::vector<ScreenInfo> screens_;

  // TODO: Add DISALLOW_COPY_AND_ASSIGN when macro is available
};

}  // namespace openscreen

#endif  // API_SCREEN_LISTENER_H_
