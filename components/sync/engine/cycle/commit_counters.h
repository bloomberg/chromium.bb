// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_COMMIT_COUNTERS_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_COMMIT_COUNTERS_H_

#include <memory>
#include <string>

#include "base/values.h"

namespace syncer {

// A class to maintain counts related to sync commit requests and responses.
struct CommitCounters {
  CommitCounters();
  ~CommitCounters();

  std::unique_ptr<base::DictionaryValue> ToValue() const;
  std::string ToString() const;

  // Counters updated before sending a commit message to the server.
  int num_creation_commits_attempted;
  int num_deletion_commits_attempted;
  int num_update_commits_attempted;

  // Counters updated based on the response from the server.
  int num_commits_success;
  int num_commits_conflict;
  int num_commits_error;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_COMMIT_COUNTERS_H_
