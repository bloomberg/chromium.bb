// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class ResourceCache;

// Manages cloud policy for components.
//
// This class takes care of fetching, validating, storing and updating policy
// for components. The components to manage have to be explicitly registered.
class ComponentCloudPolicyService : public CloudPolicyClient::Observer,
                                    public CloudPolicyStore::Observer {
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
  ComponentCloudPolicyService(Delegate* delegate,
                              CloudPolicyStore* store,
                              scoped_ptr<ResourceCache> cache);
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

  // |component_ids| is the complete set of components to track for the given
  // |domain|. This purges unused components from the cache, and starts
  // updating the components in |component_ids|.
  // It's only valid to call this for domains that are supported, i.e.
  // SupportsDomain(domain) is true.
  void RegisterPolicyDomain(PolicyDomain domain,
                            const std::set<std::string>& component_ids);

  // CloudPolicyClient::Observer implementation:
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

  // CloudPolicyStore::Observer implementation:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

 private:
  class Backend;
  typedef std::set<std::string> StringSet;
  typedef std::map<PolicyDomain, StringSet> ComponentMap;

  void InitializeBackend();
  void OnBackendInitialized(scoped_ptr<ComponentMap> components,
                            scoped_ptr<PolicyBundle> initial_policy);
  void InitializeClient();
  void OnPolicyUpdated(scoped_ptr<PolicyBundle> policy);

  void SetCredentialsAndReloadClient();
  bool UpdateClientNamespaces(PolicyDomain domain,
                              const StringSet& old_set,
                              const StringSet& new_set);
  void AddNamespacesToFetch(PolicyDomain domain, const StringSet& set);
  void RemoveNamespacesToFetch(PolicyDomain domain, const StringSet& set);

  Delegate* delegate_;

  // This class manages others that live on a background thread; those are
  // managed by |backend_|. |backend_| lives in the thread that backs
  // |backend_task_runner_|, but is owned by |this|. It is created on UI, but
  // from then on it only receives calls on the background thread, including
  // destruction. So it's always safe to post tasks to |backend_|, since its
  // deletion is posted after the deletion of |this|.
  Backend* backend_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  CloudPolicyClient* client_;
  CloudPolicyStore* store_;

  // The currently registered components for each policy domain. If a policy
  // domain doesn't have an entry in this map then it hasn't registered its
  // component yet. A domain in this map with an empty set of components means
  // that the domain is registered, but has no components.
  ComponentMap registered_components_;

  // Contains all the current policies for components.
  PolicyBundle policy_;

  bool is_initialized_;
  base::WeakPtrFactory<ComponentCloudPolicyService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ComponentCloudPolicyService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_
