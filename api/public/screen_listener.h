// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_SCREEN_LISTENER_H_
#define API_PUBLIC_SCREEN_LISTENER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "api/public/screen_info.h"
#include "base/macros.h"

namespace openscreen {

// Used to report an error from a ScreenListener implementation.
struct ScreenListenerError {
 public:
  // TODO: Add additional error types, as implementations progress.
  enum class Code {
    kNone = 0,
  };

  ScreenListenerError();
  ScreenListenerError(Code error, const std::string& message);
  ScreenListenerError(const ScreenListenerError& other);
  ~ScreenListenerError();

  ScreenListenerError& operator=(const ScreenListenerError& other);

  Code error;
  std::string message;
};

class ScreenListener {
 public:
  enum class State {
    kStopped = 0,
    kStarting,
    kRunning,
    kStopping,
    kSearching,
    kSuspended,
  };

  // Microseconds after the epoch.
  // TODO: Replace with a base::Time object.
  typedef uint64_t timestamp_t;

  // Holds a set of metrics, captured over a specific range of time, about the
  // behavior of a ScreenListener instance.
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

    // The maximum number of screens discovered over the timestamp range.  The
    // latter two fields break this down by screens advertising ipv4 and ipv6
    // endpoints.
    size_t num_screens = 0;
    size_t num_ipv4_screens = 0;
    size_t num_ipv6_screens = 0;
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
    // Called when the state becomes kSearching.
    virtual void OnSearching() = 0;

    // Notifications to changes to the listener's screen list.
    virtual void OnScreenAdded(const ScreenInfo&) = 0;
    virtual void OnScreenChanged(const ScreenInfo&) = 0;
    virtual void OnScreenRemoved(const ScreenInfo&) = 0;
    // Called if all screens are no longer available, e.g. all network
    // interfaces have been disabled.
    virtual void OnAllScreensRemoved() = 0;

    // Reports an error.
    virtual void OnError(ScreenListenerError) = 0;

    // Reports metrics.
    virtual void OnMetrics(Metrics) = 0;
  };

  virtual ~ScreenListener();

  // Starts listening for screens using the config object.
  // Returns true if state() == kStopped and the service will be started, false
  // otherwise.
  virtual bool Start() = 0;

  // Starts the listener in kSuspended mode.  This could be used to enable
  // immediate search via SearchNow() in the future.
  // Returns true if state() == kStopped and the service will be started, false
  // otherwise.
  virtual bool StartAndSuspend() = 0;

  // Stops listening and cancels any search in progress.
  // Returns true if state() != (kStopped|kStopping).
  virtual bool Stop() = 0;

  // Suspends background listening. For example, the tab wanting screen
  // availability might go in the background, meaning we can suspend listening
  // to save power.
  // Returns true if state() == (kRunning|kSearching|kStarting), meaning the
  // suspension will take effect.
  virtual bool Suspend() = 0;

  // Resumes listening.  Returns true if state() == (kSuspended|kSearching).
  virtual bool Resume() = 0;

  // Asks the listener to search for screens now, even if the listener is
  // is currently suspended.  If a background search is already in
  // progress, this has no effect.  Returns true if state() ==
  // (kRunning|kSuspended).
  virtual bool SearchNow() = 0;

  // Returns the current state of the listener.
  State state() const { return state_; }

  // Returns the last error reported by this listener.
  const ScreenListenerError& last_error() const { return last_error_; }

  // Returns the current list of screens known to the ScreenListener.
  virtual const std::vector<ScreenInfo>& GetScreens() const = 0;

 protected:
  explicit ScreenListener(Observer* observer);

  State state_ = State::kStopped;
  ScreenListenerError last_error_;
  Observer* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ScreenListener);
};

}  // namespace openscreen

#endif  // API_PUBLIC_SCREEN_LISTENER_H_
