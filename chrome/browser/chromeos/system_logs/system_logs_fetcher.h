// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"

namespace chromeos {

typedef std::map<std::string, std::string> SystemLogsResponse;

// Callback that the data sources use to return data.
typedef base::Callback<void(SystemLogsResponse* response)>
    SysLogsSourceCallback;

// Callback that the SystemLogsFetcher uses to return data.
typedef base::Callback<void(scoped_ptr<SystemLogsResponse> response)>
    SysLogsFetcherCallback;

// The SystemLogsSource provides a interface for the data sources that
// the SystemLogsFetcher class uses to fetch logs and other
// information.
class SystemLogsSource {
 public:
  // Fetches data and passes it by to the callback
  virtual void Fetch(const SysLogsSourceCallback& callback) = 0;
  virtual ~SystemLogsSource() {}
};

// The SystemLogsFetcher creates a list of data sources which must be
// classes that implement the SystemLogsSource. It's Fetch function
// receives as a parameter a callback that takes only one parameter
// SystemLogsResponse that is a map of keys and values.
// Each data source also returns a SystemLogsResponse. If two data sources
// return the same key, only the first one will be stored.
// The class runs on the UI thread.
//
// EXAMPLE:
// class Example {
//  public:
//   void ProcessLogs(SystemLogsResponse* response) {
//      //do something with the logs
//   }
//   void GetLogs() {
//     SystemLogsFetcher* fetcher = new SystemLogsFetcher();
//     fetcher->Fetch(base::Bind(&Example::ProcessLogs, this));
//   }
class SystemLogsFetcher {
 public:
  SystemLogsFetcher();
  ~SystemLogsFetcher();

  void Fetch(const SysLogsFetcherCallback& callback);

 private:
  // Callback passed to all the data sources. It merges the |data| it recieves
  // into response_. When all the data sources have responded, it deletes their
  // objects and returns the response to the callback_. After this it
  // deletes this instance of the object.
  void AddResponse(SystemLogsResponse* response);

  ScopedVector<SystemLogsSource> data_sources_;
  SysLogsFetcherCallback callback_;

  scoped_ptr<SystemLogsResponse> response_;  // The actual response data.
  size_t num_pending_requests_;   // The number of callbacks it should get.

  base::WeakPtrFactory<SystemLogsFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemLogsFetcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_H_

