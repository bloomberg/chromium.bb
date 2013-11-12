// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "chrome/browser/policy/policy_bundle.h"
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
// for components. The components to manage have to be explicitly registered.
class ComponentCloudPolicyService : public CloudPolicyClient::Observer,
                                    public CloudPolicyStore::Observer,
                                    public base::NonThreadSafe {
 public:
  // Key for the ResourceCache where the list of known components is cached.
  static const char kComponentNamespaceCache[];

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

  // |store| is used to get the current DMToken and the username.
  // |cache| is used to load and store local copies of the downloaded policies.
  // Download scheduling, validation and caching of policies are done via the
  // |backend_task_runner|, which must support file I/O. Network I/O is done via
  // the |io_task_runner|.
  ComponentCloudPolicyService(
      Delegate* delegate,
      CloudPolicyStore* store,
      scoped_ptr<ResourceCache> cache,
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

  // Connects to the cloud policy service using |client|. |client| must outlive
  // this object. Only cached policies will be served until a |client| is
  // connected.
  // |request_context| is used with the URLFetchers triggered by the updater.
  void Connect(CloudPolicyClient* client,
               scoped_refptr<net::URLRequestContextGetter> request_context);

  // Disconnects from the cloud policy service and stops trying to download
  // remote policy data.
  void Disconnect();

  // |schema_map| contains the schemas for all the components that this
  // service should load policy for.
  // This purges unused components from the cache, and starts updating the
  // components listed in the map.
  // Unsupported domains in the map are ignored.
  void OnSchemasUpdated(const scoped_refptr<SchemaMap>& schema_map);

  // CloudPolicyClient::Observer implementation:
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

  // CloudPolicyStore::Observer implementation:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

 private:
  class Backend;
  typedef std::set<PolicyNamespaceKey> PolicyNamespaceKeys;

  void InitializeBackend();
  void OnBackendInitialized(scoped_ptr<PolicyNamespaceKeys> initial_keys,
                            scoped_ptr<PolicyBundle> initial_policy);
  void InitializeClient();
  void OnPolicyUpdated(scoped_ptr<PolicyBundle> policy);

  void SetCredentialsAndReloadClient();
  bool UpdateClientNamespaces(const PolicyNamespaceKeys& old_keys,
                              const PolicyNamespaceKeys& new_keys);
  void AddNamespacesToFetch(const PolicyNamespaceKeys& keys);
  void RemoveNamespacesToFetch(const PolicyNamespaceKeys& keys);

  Delegate* delegate_;

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

  CloudPolicyClient* client_;
  CloudPolicyStore* store_;

  // The currently registered components for each policy domain.
  PolicyNamespaceKeys keys_;

  // Contains all the current policies for components.
  PolicyBundle policy_;

  bool is_initialized_;
  bool has_initial_keys_;
  base::WeakPtrFactory<ComponentCloudPolicyService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ComponentCloudPolicyService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_
