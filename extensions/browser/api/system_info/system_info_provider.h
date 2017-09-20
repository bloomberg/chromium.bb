// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_INFO_SYSTEM_INFO_PROVIDER_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_INFO_SYSTEM_INFO_PROVIDER_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace base {
class SequencedTaskRunner;
}

namespace extensions {

// An abstract base class for all kinds of system information providers. Each
// kind of SystemInfoProvider is a single shared instance. It is created if
// needed, and destroyed at exit time. This is done via LazyInstance and
// scoped_refptr.
//
// The SystemInfoProvider is designed to query system information on the worker
// pool. It also maintains a queue of callbacks on the UI thread which are
// waiting for the completion of querying operation. Once the query operation
// is completed, all pending callbacks in the queue get called on the UI
// thread. In this way, it avoids frequent querying operation in case of lots
// of query requests, e.g. calling systemInfo.cpu.get repeatedly in an
// extension process.
//
// Each kind of SystemInfoProvider should satisfy an API query in a subclass on
// the blocking pool.
class SystemInfoProvider
    : public base::RefCountedThreadSafe<SystemInfoProvider> {
 public:
  // Callback type for completing to get information. The argument indicates
  // whether its contents are valid, for example, no error occurs in querying
  // the information.
  using QueryInfoCompletionCallback = base::Callback<void(bool)>;
  using CallbackQueue = base::queue<QueryInfoCompletionCallback>;

  SystemInfoProvider();

  // Override to do any prepare work on UI thread before |QueryInfo()| gets
  // called.
  virtual void PrepareQueryOnUIThread();

  // The parameter |do_query_info_callback| is query info task which is posted
  // to SystemInfoProvider sequenced worker pool.
  //
  // You can do any initial things of *InfoProvider before start to query info.
  // While overriding this method, |do_query_info_callback| *must* be called
  // directly or indirectly.
  //
  // Sample usage please refer to StorageInfoProvider.
  virtual void InitializeProvider(const base::Closure& do_query_info_callback);

  // Start to query the system information. Should be called on UI thread.
  // The |callback| will get called once the query is completed.
  //
  // If the parameter |callback| itself calls StartQueryInfo(callback2),
  // callback2 will be called immediately rather than triggering another call to
  // the system.
  void StartQueryInfo(const QueryInfoCompletionCallback& callback);

 protected:
  virtual ~SystemInfoProvider();

 private:
  friend class base::RefCountedThreadSafe<SystemInfoProvider>;

  // Interface to query the system information synchronously.
  // Return true if no error occurs.
  // Should be called in the blocking pool.
  virtual bool QueryInfo() = 0;

  // Called on UI thread. The |success| parameter means whether it succeeds
  // to get the information.
  void OnQueryCompleted(bool success);

  void StartQueryInfoPostInitialization();

  // The queue of callbacks waiting for the info querying completion. It is
  // maintained on the UI thread.
  CallbackQueue callbacks_;

  // Indicates if it is waiting for the querying completion.
  bool is_waiting_for_completion_;

  // Sequenced task runner to safely query system information.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SystemInfoProvider);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_INFO_SYSTEM_INFO_PROVIDER_H_
