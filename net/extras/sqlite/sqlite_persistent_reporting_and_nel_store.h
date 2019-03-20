// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_REPORTING_AND_NEL_STORE_H_
#define NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_REPORTING_AND_NEL_STORE_H_

#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/network_error_logging/persistent_reporting_and_nel_store.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace net {

class COMPONENT_EXPORT(NET_EXTRAS) SQLitePersistentReportingAndNELStore
    : public PersistentReportingAndNELStore {
 public:
  SQLitePersistentReportingAndNELStore(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);

  ~SQLitePersistentReportingAndNELStore() override;

  // NetworkErrorLoggingService::PersistentNELStore implementation
  void LoadNELPolicies(NELPoliciesLoadedCallback loaded_callback) override;
  void AddNELPolicy(
      const NetworkErrorLoggingService::NELPolicy& policy) override;
  void UpdateNELPolicyAccessTime(
      const NetworkErrorLoggingService::NELPolicy& policy) override;
  void DeleteNELPolicy(
      const NetworkErrorLoggingService::NELPolicy& policy) override;
  void Flush() override;

  size_t GetQueueLengthForTesting() const;

 private:
  class Backend;

  // Calls |callback| with the loaded |policies|.
  void CompleteLoadNELPolicies(
      NELPoliciesLoadedCallback callback,
      std::vector<NetworkErrorLoggingService::NELPolicy> policies);

  const scoped_refptr<Backend> backend_;

  base::WeakPtrFactory<SQLitePersistentReportingAndNELStore> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SQLitePersistentReportingAndNELStore);
};

}  // namespace net

#endif  // NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_REPORTING_AND_NEL_STORE_H_
