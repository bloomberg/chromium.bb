// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CLOUD_EXTERNAL_DATA_MANAGER_BASE_H_
#define CHROME_BROWSER_POLICY_CLOUD_CLOUD_EXTERNAL_DATA_MANAGER_BASE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/cloud/cloud_external_data_manager.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class CloudExternalDataStore;
struct PolicyDefinitionList;

// Downloads, verifies, caches and retrieves external data referenced by
// policies.
// This is a common base class used by specializations for regular users and
// device-local accounts.
class CloudExternalDataManagerBase : public CloudExternalDataManager {
 public:
  // The |policy_definitions| are used to determine the maximum size that the
  // data referenced by each policy can have. All data download, verification,
  // caching and retrieval tasks are run via the |backend_task_runner|.
  CloudExternalDataManagerBase(
      const PolicyDefinitionList* policy_definitions,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  virtual ~CloudExternalDataManagerBase();

  // Allows downloaded external data to be cached in |external_data_store|.
  // Ownership of the store is taken. The store can be destroyed by calling
  // SetExternalDataStore(scoped_ptr<CloudExternalDataStore>()). Accesses to the
  // store are made via |backend_task_runner_| only.
  void SetExternalDataStore(
      scoped_ptr<CloudExternalDataStore> external_data_store);

  // CloudExternalDataManager:
  virtual void SetPolicyStore(CloudPolicyStore* policy_store) OVERRIDE;
  virtual void OnPolicyStoreLoaded() OVERRIDE;
  virtual void Connect(
      scoped_refptr<net::URLRequestContextGetter> request_context) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual void Fetch(
      const std::string& policy,
      const ExternalDataFetcher::FetchCallback& callback) OVERRIDE;

 protected:
  friend class CouldExternalDataManagerBaseTest;

  // Try to download and cache all external data referenced by policies in
  // |policy_store_|.
  void FetchAll();

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

 private:
  // The |backend_| handles all data download, verification, caching and
  // retrieval. It is instantiated on the UI thread but from then on, is
  // accessed via the |backend_task_runner_| only.
  class Backend;
  Backend* backend_;

  DISALLOW_COPY_AND_ASSIGN(CloudExternalDataManagerBase);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CLOUD_EXTERNAL_DATA_MANAGER_BASE_H_
