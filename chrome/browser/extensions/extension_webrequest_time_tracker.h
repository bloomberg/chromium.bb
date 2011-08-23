// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_TIME_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_TIME_TRACKER_H_
#pragma once

#include <map>
#include <queue>
#include <set>
#include <string>

#include "base/time.h"
#include "base/gtest_prod_util.h"
#include "googleurl/src/gurl.h"

namespace base {
class Time;
}

// This class keeps monitors how much delay extensions add to network requests
// by using the webRequest API. If the delay is sufficient, we will warn the
// user that extensions are slowing down the browser.
class ExtensionWebRequestTimeTracker {
 public:
  ExtensionWebRequestTimeTracker();
  ~ExtensionWebRequestTimeTracker();

  // Records the time that a request was created.
  void LogRequestStartTime(int64 request_id, const base::Time& start_time,
                           const GURL& url);

  // Records the time that a request either completed or encountered an error.
  void LogRequestEndTime(int64 request_id, const base::Time& end_time);

  // Records an additional delay for the given request caused by the given
  // extension.
  void IncrementExtensionBlockTime(
      const std::string& extension_id,
      int64 request_id,
      const base::TimeDelta& block_time);

  // Records an additional delay for the given request caused by all extensions
  // combined.
  void IncrementTotalBlockTime(
      int64 request_id,
      const base::TimeDelta& block_time);

  // Called when an extension has canceled the given request.
  void SetRequestCanceled(int64 request_id);

  // Called when an extension has redirected the given request to another URL.
  void SetRequestRedirected(int64 request_id);

 private:
  // Timing information for a single request.
  struct RequestTimeLog {
    GURL url;  // used for debug purposes only
    bool completed;
    base::Time request_start_time;
    base::TimeDelta request_duration;
    base::TimeDelta block_duration;
    std::map<std::string, base::TimeDelta> extension_block_durations;
    RequestTimeLog();
    ~RequestTimeLog();
  };

  // Called after a request finishes, to analyze the delays and warn the user
  // if necessary.
  void Analyze(int64 request_id);

  // A map of recent request IDs to timing info for each request.
  std::map<int64, RequestTimeLog> request_time_logs_;

  // A list of recent request IDs that we know about. Used to limit the size of
  // the logs.
  std::queue<int64> request_ids_;

  // The set of recent requests that have been delayed either a large or
  // moderate amount by extensions.
  std::set<int64> excessive_delays_;
  std::set<int64> moderate_delays_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTimeTrackerTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTimeTrackerTest,
                           IgnoreFastRequests);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTimeTrackerTest,
                           CancelOrRedirect);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTimeTrackerTest, Delays);

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebRequestTimeTracker);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_TIME_TRACKER_H_
