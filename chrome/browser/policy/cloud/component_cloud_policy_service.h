// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer/timer.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/schema_registry.h"
#include "components/policy/core/common/policy_namespace.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class ExternalPolicyDataFetcherBackend;
class ResourceCache;
class SchemaMap;

// Manages cloud policy for components.
//
// This class takes care of fetching, validating, storing and updating policy
// for components. The components to manage come from a SchemaRegistry.
class ComponentCloudPolicyService : public CloudPolicyClient::Observer,
                                    public CloudPolicyStore::Observer,
                                    public SchemaRegistry::Observer,
                                    public base::NonThreadSafe {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Invoked whenever the service has appended new namespaces to fetch to
    // the CloudPolicyClient, signaling that a policy fetch should be done soon.
    virtual void OnComponentCloudPolicyRefreshNeeded() = 0;

    // Invoked whenever the policy served by policy() changes. This is also
    // invoked for the first time once the backend is initialized, and
    // is_initialized() becomes true.
    virtual void OnComponentCloudPolicyUpdated() = 0;
  };

  // All of these components must outlive this instance.
  //
  // The |delegate| is notified of updates to the downloaded policies, and is
  // notified whenever a refresh is needed.
  // |schema_registry| contains the list of components to fetch policy for.
  // |store| is used to get the current DMToken and the username.
  // |cache| is used to load and store local copies of the downloaded policies.
  // Download scheduling, validation and caching of policies are done via the
  // |backend_task_runner|, which must support file I/O. Network I/O is done via
  // the |io_task_runner|.
  // |client| is updated with the list of components to fetch.
  // |request_context| is used by the background URLFetchers.
  ComponentCloudPolicyService(
      Delegate* delegate,
      SchemaRegistry* schema_registry,
      CloudPolicyStore* store,
      scoped_ptr<ResourceCache> cache,
      CloudPolicyClient* client,
      scoped_refptr<net::URLRequestContextGetter> request_context,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  virtual ~ComponentCloudPolicyService();

  // Returns true if |domain| is supported by the service.
  static bool SupportsDomain(PolicyDomain domain);

  // Returns true if the backend is initialized, and the initial policies and
  // components are being served.
  bool is_initialized() const { return is_initialized_; }

  // Returns the current policies for components.
  const PolicyBundle& policy() const { return policy_; }

  // CloudPolicyClient::Observer implementation:
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

  // CloudPolicyStore::Observer implementation:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

  // SchemaRegistry::Observer implementation:
  virtual void OnSchemaRegistryReady() OVERRIDE;
  virtual void OnSchemaRegistryUpdated(bool has_new_schemas) OVERRIDE;

 private:
  class Backend;

  void InitializeIfReady();
  void OnBackendInitialized(scoped_ptr<PolicyBundle> initial_policy);
  void SetCurrentSchema(const scoped_refptr<SchemaMap>& new_schema_map,
                        bool send_to_backend);
  void OnPolicyUpdated(scoped_ptr<PolicyBundle> policy);

  Delegate* delegate_;
  SchemaRegistry* schema_registry_;
  CloudPolicyStore* store_;
  CloudPolicyClient* client_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // The |external_policy_data_fetcher_backend_| handles network I/O for the
  // |backend_| because URLRequestContextGetter and URLFetchers cannot be
  // referenced from background threads. It is instantiated on the thread |this|
  // runs on but after that, must only be accessed and eventually destroyed via
  // the |io_task_runner_|.
  scoped_ptr<ExternalPolicyDataFetcherBackend>
      external_policy_data_fetcher_backend_;

  // The |backend_| handles all download scheduling, validation and caching of
  // policies. It is instantiated on the thread |this| runs on but after that,
  // must only be accessed and eventually destroyed via the
  // |backend_task_runner_|.
  scoped_ptr<Backend> backend_;

  // The currently registered components for each policy domain. Used to
  // determine which components changed when a new SchemaMap becomes
  // available.
  scoped_refptr<SchemaMap> current_schema_map_;

  // Contains all the current policies for components.
  PolicyBundle policy_;

  // Used to delay changes triggered by updates to the SchemaRegistry. See
  // the implementation of OnSchemaRegistryUpdated() for details.
  base::OneShotTimer<ComponentCloudPolicyService> schema_update_timer_;

  bool is_initialized_;
  bool has_credentials_;
  base::WeakPtrFactory<ComponentCloudPolicyService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ComponentCloudPolicyService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_
