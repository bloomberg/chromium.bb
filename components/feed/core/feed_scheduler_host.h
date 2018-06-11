// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_SCHEDULER_HOST_H_
#define COMPONENTS_FEED_CORE_FEED_SCHEDULER_HOST_H_

#include "base/callback.h"
#include "base/macros.h"

namespace base {
class Time;
}  // namespace base

namespace feed {

// The enum values and names are kept in sync with SchedulerApi.RequestBehavior
// through Java unit tests, new values however must be manually added. If any
// new values are added, also update FeedSchedulerBridgeTest.java.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.feed
enum NativeRequestBehavior {
  UNKNOWN = 0,
  REQUEST_WITH_WAIT,
  REQUEST_WITH_CONTENT,
  REQUEST_WITH_TIMEOUT,
  NO_REQUEST_WITH_WAIT,
  NO_REQUEST_WITH_CONTENT,
  NO_REQUEST_WITH_TIMEOUT
};

// Implementation of the Feed Scheduler Host API. The scheduler host decides
// what content is allowed to be shown, based on its age, and when to fetch new
// content.
class FeedSchedulerHost {
 public:
  FeedSchedulerHost();
  ~FeedSchedulerHost();

  // Called when the NTP is opened to decide how to handle displaying and
  // refreshing content.
  NativeRequestBehavior ShouldSessionRequestData(
      bool has_content,
      base::Time content_creation_date_time,
      bool has_outstanding_request);

  // Called when a successful refresh completes.
  void OnReceiveNewContent(base::Time content_creation_date_time);

  // Called when an unsuccessful refresh completes.
  void OnRequestError(int network_response_code);

  // Registers a callback to trigger a refresh.
  void RegisterTriggerRefreshCallback(base::RepeatingClosure callback);

 private:
  base::RepeatingClosure trigger_refresh_;

  DISALLOW_COPY_AND_ASSIGN(FeedSchedulerHost);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_SCHEDULER_HOST_H_
