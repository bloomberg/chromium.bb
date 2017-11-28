// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_H_
#define COMPONENTS_FEEDBACK_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/feedback/anonymizer_tool.h"
#include "components/feedback/feedback_common.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Callback that the SystemLogsFetcher uses to return data.
using SysLogsFetcherCallback =
    base::Callback<void(std::unique_ptr<SystemLogsResponse>)>;

// The SystemLogsFetcher fetches key-value data from a list of log sources.
//
// EXAMPLE:
// class Example {
//  public:
//   void ProcessLogs(std::unique_ptr<SystemLogsResponse> response) {
//      // do something with the logs
//   }
//   void GetLogs() {
//     SystemLogsFetcher* fetcher = new SystemLogsFetcher(/*scrub_data=*/ true);
//     fetcher->AddSource(base::MakeUnique<LogSourceOne>());
//     fetcher->AddSource(base::MakeUnique<LogSourceTwo>());
//     fetcher->Fetch(base::Bind(&Example::ProcessLogs, this));
//   }
// };
class SystemLogsFetcher {
 public:
  // If scrub_data is true, logs will be anonymized.
  explicit SystemLogsFetcher(bool scrub_data);
  ~SystemLogsFetcher();

  // Adds a source to use when fetching.
  void AddSource(std::unique_ptr<SystemLogsSource> source);

  // Starts the fetch process. After the fetch completes, this instance calls
  // |callback|, then schedules itself to be deleted.
  void Fetch(const SysLogsFetcherCallback& callback);

 private:
  // Callback passed to all the data sources. May call Scrub(), then calls
  // AddResponse().
  void OnFetched(const std::string& source_name,
                 std::unique_ptr<SystemLogsResponse> response);

  // Merges the |response| it receives into response_. When all the data sources
  // have responded, it deletes their objects and returns the response to the
  // callback_. After this it deletes this instance of the object.
  void AddResponse(const std::string& source_name,
                   std::unique_ptr<SystemLogsResponse> response);

  std::vector<std::unique_ptr<SystemLogsSource>> data_sources_;
  SysLogsFetcherCallback callback_;

  std::unique_ptr<SystemLogsResponse> response_;  // The actual response data.
  size_t num_pending_requests_;  // The number of callbacks it should get.

  std::unique_ptr<feedback::AnonymizerTool> anonymizer_;

  base::WeakPtrFactory<SystemLogsFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemLogsFetcher);
};

}  // namespace system_logs

#endif  // COMPONENTS_FEEDBACK_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_H_
