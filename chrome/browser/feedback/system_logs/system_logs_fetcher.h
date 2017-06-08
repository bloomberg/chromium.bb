// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_H_

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

namespace system_logs {

using SystemLogsResponse = FeedbackCommon::SystemLogsMap;

// Callback that the data sources use to return data.
using SysLogsSourceCallback = base::Callback<void(SystemLogsResponse*)>;

// Callback that the SystemLogsFetcher uses to return data.
using SysLogsFetcherCallback =
    base::Callback<void(std::unique_ptr<SystemLogsResponse>)>;

// The SystemLogsSource provides an interface for the data sources that
// the SystemLogsFetcher class uses to fetch logs and other information.
class SystemLogsSource {
 public:
  // |source_name| provides a descriptive identifier for debugging.
  explicit SystemLogsSource(const std::string& source_name);
  virtual ~SystemLogsSource();

  // Fetches data and passes it by pointer to the callback
  virtual void Fetch(const SysLogsSourceCallback& callback) = 0;

  const std::string& source_name() const { return source_name_; }

 private:
  std::string source_name_;
};

// The SystemLogsFetcher fetches key-value data from a list of log sources.
//
// EXAMPLE:
// class Example {
//  public:
//   void ProcessLogs(SystemLogsResponse* response) {
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
  // TODO(battre): This class needs to be expanded to provide better scrubbing
  // of system logs.
  explicit SystemLogsFetcher(bool scrub_data);
  ~SystemLogsFetcher();

  // Adds a source to use when fetching.
  void AddSource(std::unique_ptr<SystemLogsSource> source);

  // Starts the fetch process.
  void Fetch(const SysLogsFetcherCallback& callback);

 private:
  // Callback passed to all the data sources. May call Scrub(), then calls
  // AddResponse().
  void OnFetched(const std::string& source_name, SystemLogsResponse* response);

  // Anonymizes the response data.
  void Scrub(SystemLogsResponse* response);

  // Merges the |response| it receives into response_. When all the data sources
  // have responded, it deletes their objects and returns the response to the
  // callback_. After this it deletes this instance of the object.
  void AddResponse(const std::string& source_name,
                   SystemLogsResponse* response);

  std::vector<std::unique_ptr<SystemLogsSource>> data_sources_;
  SysLogsFetcherCallback callback_;

  std::unique_ptr<SystemLogsResponse> response_;  // The actual response data.
  size_t num_pending_requests_;  // The number of callbacks it should get.

  std::unique_ptr<feedback::AnonymizerTool> anonymizer_;

  base::WeakPtrFactory<SystemLogsFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemLogsFetcher);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_H_
