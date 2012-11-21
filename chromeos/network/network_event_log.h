// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_EVENT_LOG_H_
#define CHROMEOS_NETWORK_NETWORK_EVENT_LOG_H_

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/time.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// Namespace for functions for logging network events.
namespace network_event_log {

// Used to determine which order to output event entries in GetAsString.
enum StringOrder {
  OLDEST_FIRST,
  NEWEST_FIRST
};

// Maximum number of event log entries, exported for testing.
CHROMEOS_EXPORT extern const size_t kMaxNetworkEventLogEntries;

// Initializes / shuts down network event logging. Calling Initialize more than
// once will reset the log.
CHROMEOS_EXPORT void Initialize();
CHROMEOS_EXPORT void Shutdown();

// Returns true if network event logging has been initialized.
CHROMEOS_EXPORT bool IsInitialized();

// Adds an entry to the event log. A maximum number of events are recorded
// after which new events replace old ones. Does nothing unless Initialize()
// has been called.
CHROMEOS_EXPORT void AddEntry(const std::string& module,
                              const std::string& event,
                              const std::string& description);

// Outputs the log to a formatted string. |order| determines which order to
// output the events. If |max_events| > 0, limits how many events are output.
CHROMEOS_EXPORT std::string GetAsString(StringOrder order, size_t max_events);

}  // namespace network_event_log

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_EVENT_LOG_H_
