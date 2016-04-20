// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_export.h"

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
class POLICY_EXPORT ComponentCloudPolicyService
    : public CloudPolicyClient::Observer,
      public CloudPolicyCore::Observer,
      public CloudPolicyStore::Observer,
      public SchemaRegistry::Observer,
      public base::NonThreadSafe {
 public:
  class POLICY_EXPORT Delegate {
   public:
    virtual ~Delegate();

    // Invoked whenever the policy served by policy() changes. This is also
    // invoked for the first time once the backend is initialized, and
    // is_initialized() becomes true.
    virtual void OnComponentCloudPolicyUpdated() = 0;
  };

  // The |delegate| is notified of updates to the downloaded policies and must
  // outlive this object.
  //
  // |schema_registry| is used to get the list of components to fetch cloud
  // policy for. It must outlive this object.
  //
  // |core| is used to obtain the CloudPolicyStore and CloudPolicyClient used
  // by this service. The store will be the source of the registration status
  // and registration credentials; the client will be used to fetch cloud
  // policy. It must outlive this object.
  //
  // The |core| MUST not be connected yet when this service is created;
  // |client| must be the client that will be connected to the |core|. This
  // is important to make sure that this service appends any necessary policy
  // fetch types to the |client| before the |core| gets connected and before
  // the initial policy fetch request is sent out.
  //
  // |cache| is used to load and store local copies of the downloaded policies.
  //
  // Download scheduling, validation and caching of policies are done via the
  // |backend_task_runner|, which must support file I/O. Network I/O is done via
  // the |io_task_runner|.
  //
  // |request_context| is used by the background URLFetchers.
  ComponentCloudPolicyService(
      Delegate* delegate,
      SchemaRegistry* schema_registry,
      CloudPolicyCore* core,
      CloudPolicyClient* client,
#if !defined(OS_ANDROID) && !defined(OS_IOS)
      std::unique_ptr<ResourceCache> cache,
#endif
      scoped_refptr<net::URLRequestContextGetter> request_context,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~ComponentCloudPolicyService() override;

  // Returns true if |domain| is supported by the service.
  static bool SupportsDomain(PolicyDomain domain);

  // Returns true if the backend is initialized, and the initial policies and
  // components are being served.
  bool is_initialized() const { return loaded_initial_policy_; }

  // Returns the current policies for components.
  const PolicyBundle& policy() const { return policy_; }

  // Deletes all the cached component policy.
  void ClearCache();

  // SchemaRegistry::Observer implementation:
  void OnSchemaRegistryReady() override;
  void OnSchemaRegistryUpdated(bool has_new_schemas) override;

  // CloudPolicyCore::Observer implementation:
  void OnCoreConnected(CloudPolicyCore* core) override;
  void OnCoreDisconnecting(CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override;

  // CloudPolicyStore::Observer implementation:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  // CloudPolicyClient::Observer implementation:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

 private:
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  class Backend;

  void InitializeIfReady();
  void OnBackendInitialized(std::unique_ptr<PolicyBundle> initial_policy);
  void ReloadSchema();
  void OnPolicyUpdated(std::unique_ptr<PolicyBundle> policy);

  Delegate* delegate_;
  SchemaRegistry* schema_registry_;
  CloudPolicyCore* core_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // The |external_policy_data_fetcher_backend_| handles network I/O for the
  // |backend_| because URLRequestContextGetter and URLFetchers cannot be
  // referenced from background threads. It is instantiated on the thread |this|
  // runs on but after that, must only be accessed and eventually destroyed via
  // the |io_task_runner_|.
  std::unique_ptr<ExternalPolicyDataFetcherBackend>
      external_policy_data_fetcher_backend_;

  // The |backend_| handles all download scheduling, validation and caching of
  // policies. It is instantiated on the thread |this| runs on but after that,
  // must only be accessed and eventually destroyed via the
  // |backend_task_runner_|.
  std::unique_ptr<Backend> backend_;

  // The currently registered components for each policy domain. Used to
  // determine which components changed when a new SchemaMap becomes
  // available.
  scoped_refptr<SchemaMap> current_schema_map_;
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

  // Contains all the policies loaded from the store, before having been
  // filtered by the |current_schema_map_|.
  std::unique_ptr<PolicyBundle> unfiltered_policy_;

  // Contains all the current policies for components, filtered by the
  // |current_schema_map_|.
  PolicyBundle policy_;

  // Whether the backend has started initializing asynchronously. Used to
  // prevent double initialization, since both OnSchemaRegistryUpdated() and
  // OnStoreLoaded() can happen while the backend is initializing.
  bool started_loading_initial_policy_;

  // Whether the backend has been initialized with the initial credentials and
  // schemas, and this provider is serving the initial policies loaded from the
  // cache.
  bool loaded_initial_policy_;

  // True if the backend currently has valid cloud policy credentials. This
  // can go back to false if the user signs out, and back again to true if the
  // user signs in again.
  bool is_registered_for_cloud_policy_;

  base::WeakPtrFactory<ComponentCloudPolicyService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ComponentCloudPolicyService);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_
