// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_SYSTEM_SESSION_ANALYZER_WIN_H_
#define COMPONENTS_BROWSER_WATCHER_SYSTEM_SESSION_ANALYZER_WIN_H_

#include <map>
#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"

namespace browser_watcher {

// Analyzes system session events for unclean sessions. Initialization is
// expensive and therefore done lazily, as the analyzer is instantiated before
// knowing whether it will be used.
class SystemSessionAnalyzer {
 public:
  enum Status {
    FAILED = 0,
    CLEAN = 1,
    UNCLEAN = 2,
    OUTSIDE_RANGE = 3,
  };

  // Minimal information about a log event.
  struct EventInfo {
    uint16_t event_id;
    base::Time event_time;
  };

  // Creates a SystemSessionAnalyzer that will analyze system sessions based on
  // events pertaining to the last session_cnt system sessions.
  explicit SystemSessionAnalyzer(uint32_t session_cnt);
  virtual ~SystemSessionAnalyzer();

  // Returns an analysis status for the system session that contains timestamp.
  virtual Status IsSessionUnclean(base::Time timestamp);

 protected:
  // Queries for events pertaining to the last session_cnt sessions. On success,
  // returns true and event_infos contains events ordered from newest to oldest.
  // Returns false otherwise. Virtual for unit testing.
  virtual bool FetchEvents(std::vector<EventInfo>* event_infos) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SystemSessionAnalyzerTest, FetchEvents);

  bool Initialize();

  // The number of sessions to query events for.
  uint32_t session_cnt_;

  bool initialized_ = false;
  bool init_success_ = false;

  // Information about unclean sessions: start time to duration until the next
  // session start, ie *not* session duration. Note: it's easier to get the
  // delta to the next session start, and changes nothing wrt classifying
  // events that occur during sessions assuming query timestamps fall within
  // system sessions.
  std::map<base::Time, base::TimeDelta> unclean_sessions_;

  // Timestamp of the oldest event.
  base::Time coverage_start_;

  DISALLOW_COPY_AND_ASSIGN(SystemSessionAnalyzer);
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_SYSTEM_SESSION_ANALYZER_WIN_H_
