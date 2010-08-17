// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REQUEST_THROTTLER_REQUEST_THROTTLER_ENTRY_H_
#define NET_REQUEST_THROTTLER_REQUEST_THROTTLER_ENTRY_H_

#include "net/request_throttler/request_throttler_entry_interface.h"

#include <string>

#include "base/ref_counted.h"
#include "base/time.h"

// Represents an entry of the Request Throttler Manager.
class RequestThrottlerEntry : public RequestThrottlerEntryInterface {
 public:
  // Additional constant to adjust back-off.
  static const int kAdditionalConstantMs;

  // Time after which the entry is considered outdated.
  static const int kEntryLifetimeSec;

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  static const double kJitterFactor;

  // Initial delay.
  static const int kInitialBackoffMs;

  // Maximum amount of time we are willing to delay our request.
  static const int kMaximumBackoffMs;

  // Factor by which the waiting time will be multiplied.
  static const double kMultiplyFactor;

  // Name of the header that servers can use to ask clients to delay their next
  // request. ex: "X-Retry-After"
  static const char kRetryHeaderName[];

  RequestThrottlerEntry();

  ////// Implementation of the Request throttler Interface. ///////

  // This method needs to be called prior to every request; if it returns
  // false, the calling module must cancel its current request.
  virtual bool IsRequestAllowed() const;

  // This method needs to be called each time a response is received.
  virtual void UpdateWithResponse(
      const RequestThrottlerHeaderInterface* response);

  ////////// Specific method of Request throttler Entry ////////////////

  // Used by the manager, returns if the entry needs to be garbage collected.
  bool IsEntryOutdated() const;

  // Used by the manager, enables the manager to flag the last successful
  // request as a failure.
  void ReceivedContentWasMalformed();

 protected:

  // This struct is used to save the state of the entry each time we are updated
  // with a response header so we can regenerate it if we are informed that one
  // of our bodies was malformed.
  struct OldValues {
    base::TimeTicks release_time;
    int number_of_failed_requests;
  };

  virtual ~RequestThrottlerEntry();

  // Calculates when we should start sending requests again. Follows a failure
  // response.
  base::TimeTicks CalculateReleaseTime();

  // Equivalent to TimeTicks::Now(), virtual to be mockable for testing purpose.
  virtual base::TimeTicks GetTimeNow() const;

  // Is used internally to increase release time following a retry-after header.
  void HandleCustomRetryAfter(const std::string& header_value);

  // Saves the state of the object to be able to regenerate it.
  // Must be informed of the state of the response.
  void SaveState();

  // This contains the timestamp at which we are allowed to start sending
  // requests again.
  base::TimeTicks release_time_;

  // Number of times we were delayed.
  int num_times_delayed_;

  // Are we currently managing this request.
  bool is_managed_;

  OldValues old_values_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestThrottlerEntry);
};

#endif  // NET_REQUEST_THROTTLER_REQUEST_THROTTLER_ENTRY_H_
