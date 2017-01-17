// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_BASE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_BASE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "components/feedback/feedback_common.h"

namespace system_logs {

using SystemLogsResponse = FeedbackCommon::SystemLogsMap;

// Callback that the data sources use to return data.
using SysLogsSourceCallback = base::Callback<void(SystemLogsResponse*)>;

// Callback that the SystemLogsFetcherBase uses to return data.
using SysLogsFetcherCallback =
    base::Callback<void(std::unique_ptr<SystemLogsResponse>)>;

// The SystemLogsSource provides a interface for the data sources that
// the SystemLogsFetcherBase class uses to fetch logs and other
// information.
class SystemLogsSource {
 public:
  // |source_name| provides a descriptive identifier for debugging.
  explicit SystemLogsSource(const std::string& source_name);
  virtual ~SystemLogsSource();

  // Fetches data and passes it by to the callback
  virtual void Fetch(const SysLogsSourceCallback& callback) = 0;

  const std::string& source_name() const { return source_name_; }

 private:
  std::string source_name_;
};

// The SystemLogsFetcherBaseBase specifies an interface for LogFetcher classes.
// Derived LogFetcher classes aggregate the logs from a list of SystemLogSource
// classes.
//
// EXAMPLE:
// class Example {
//  public:
//   void ProcessLogs(SystemLogsResponse* response) {
//      //do something with the logs
//   }
//   void GetLogs() {
//     SystemLogsFetcherBase* fetcher = new SystemLogsFetcherBase();
//     fetcher->Fetch(base::Bind(&Example::ProcessLogs, this));
//   }
class SystemLogsFetcherBase
    : public base::SupportsWeakPtr<SystemLogsFetcherBase> {
 public:
  SystemLogsFetcherBase();
  virtual ~SystemLogsFetcherBase();

  void Fetch(const SysLogsFetcherCallback& callback);

 protected:
  // Callback passed to all the data sources. Calls Rewrite() and AddResponse().
  void OnFetched(const std::string& source_name, SystemLogsResponse* response);

  // Virtual function that allows derived classes to modify the response before
  // it gets added to the output.
  virtual void Rewrite(const std::string& source_name,
                       SystemLogsResponse* response);

  // Merges the |data| it receives into response_. When all the data sources
  // have responded, it deletes their objects and returns the response to the
  // callback_. After this it deletes this instance of the object.
  void AddResponse(const std::string& source_name,
                   SystemLogsResponse* response);

  ScopedVector<SystemLogsSource> data_sources_;
  SysLogsFetcherCallback callback_;

  std::unique_ptr<SystemLogsResponse> response_;  // The actual response data.
  size_t num_pending_requests_;   // The number of callbacks it should get.

 private:

  DISALLOW_COPY_AND_ASSIGN(SystemLogsFetcherBase);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_BASE_H_
