// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/component_cloud_policy_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/pickle.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "chrome/browser/policy/cloud/component_cloud_policy_store.h"
#include "chrome/browser/policy/cloud/component_cloud_policy_updater.h"
#include "chrome/browser/policy/cloud/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/cloud/resource_cache.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"

namespace em = enterprise_management;

namespace policy {

const char ComponentCloudPolicyService::kComponentNamespaceCache[] =
    "component-namespace-cache";

ComponentCloudPolicyService::Delegate::~Delegate() {}

// Owns the objects that live on the background thread, and posts back to UI
// to the service whenever the policy changes.
class ComponentCloudPolicyService::Backend
    : public ComponentCloudPolicyStore::Delegate {
 public:
  Backend(base::WeakPtr<ComponentCloudPolicyService> service,
          scoped_refptr<base::SequencedTaskRunner> task_runner,
          scoped_ptr<ResourceCache> cache);
  virtual ~Backend();

  // This is invoked right after the constructor but on the backend background
  // thread. Used to create the store on the right thread.
  void Init();

  // Reads the initial list of components and the initial policy.
  void FinalizeInit();

  // Creates the backend updater.
  void Connect(scoped_refptr<net::URLRequestContextGetter> request_context);

  // Stops updating remote data. Cached policies are still served.
  void Disconnect();

  // Loads the initial policies from the store. |username| and |dm_token| are
  // used to validate the cached policies.
  void SetCredentials(const std::string& username, const std::string& dm_token);

  // Passes a policy protobuf to the backend, to start its validation and
  // eventual download of the policy data on the background thread.
  // This is ignored if the backend isn't connected.
  void UpdateExternalPolicy(scoped_ptr<em::PolicyFetchResponse> response);

  // ComponentCloudPolicyStore::Delegate implementation:
  virtual void OnComponentCloudPolicyStoreUpdated() OVERRIDE;

  // Passes the current list of components in |domain|, so that the disk cache
  // can purge components that aren't being tracked anymore.
  void SetCurrentComponents(PolicyDomain domain, const StringSet* components);

 private:
  scoped_ptr<ComponentMap> ReadCachedComponents();

  base::WeakPtr<ComponentCloudPolicyService> service_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_ptr<ResourceCache> cache_;
  scoped_ptr<ComponentCloudPolicyStore> store_;
  scoped_ptr<ComponentCloudPolicyUpdater> updater_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

ComponentCloudPolicyService::Backend::Backend(
    base::WeakPtr<ComponentCloudPolicyService> service,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_ptr<ResourceCache> cache)
    : service_(service),
      task_runner_(task_runner),
      cache_(cache.Pass()) {}

ComponentCloudPolicyService::Backend::~Backend() {}

void ComponentCloudPolicyService::Backend::Init() {
  DCHECK(!store_);
  store_.reset(new ComponentCloudPolicyStore(this, cache_.get()));
}

void ComponentCloudPolicyService::Backend::FinalizeInit() {
  // Read the components that were cached in the last SetCurrentComponents()
  // calls for each domain.
  scoped_ptr<ComponentMap> components = ReadCachedComponents();

  // Read the initial policy.
  store_->Load();
  scoped_ptr<PolicyBundle> policy(new PolicyBundle);
  policy->CopyFrom(store_->policy());

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&ComponentCloudPolicyService::OnBackendInitialized,
                 service_,
                 base::Passed(&components),
                 base::Passed(&policy)));
}

void ComponentCloudPolicyService::Backend::SetCredentials(
    const std::string& username,
    const std::string& dm_token) {
  store_->SetCredentials(username, dm_token);
}

void ComponentCloudPolicyService::Backend::Connect(
    scoped_refptr<net::URLRequestContextGetter> request_context) {
  updater_.reset(new ComponentCloudPolicyUpdater(
      task_runner_, request_context, store_.get()));
}

void ComponentCloudPolicyService::Backend::Disconnect() {
  updater_.reset();
}

void ComponentCloudPolicyService::Backend::UpdateExternalPolicy(
    scoped_ptr<em::PolicyFetchResponse> response) {
  if (updater_)
    updater_->UpdateExternalPolicy(response.Pass());
}

void ComponentCloudPolicyService::Backend::
    OnComponentCloudPolicyStoreUpdated() {
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle);
  bundle->CopyFrom(store_->policy());
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&ComponentCloudPolicyService::OnPolicyUpdated,
                 service_,
                 base::Passed(&bundle)));
}

void ComponentCloudPolicyService::Backend::SetCurrentComponents(
    PolicyDomain domain,
    const StringSet* components) {
  // Store the current list of components in the cache.
  std::string policy_type;
  if (ComponentCloudPolicyStore::GetPolicyType(domain, &policy_type)) {
    Pickle pickle;
    for (StringSet::const_iterator it = components->begin();
         it != components->end(); ++it) {
      pickle.WriteString(*it);
    }
    std::string data(reinterpret_cast<const char*>(pickle.data()),
                     pickle.size());
    cache_->Store(kComponentNamespaceCache, policy_type, data);
  }

  // Purge any components that have been removed.
  if (store_)
    store_->Purge(domain, *components);
}

scoped_ptr<ComponentCloudPolicyService::ComponentMap>
    ComponentCloudPolicyService::Backend::ReadCachedComponents() {
  scoped_ptr<ComponentMap> components(new ComponentMap);
  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys(kComponentNamespaceCache, &contents);
  for (std::map<std::string, std::string>::iterator it = contents.begin();
       it != contents.end(); ++it) {
    PolicyDomain domain;
    if (ComponentCloudPolicyStore::GetPolicyDomain(it->first, &domain)) {
      StringSet& set = (*components)[domain];
      const Pickle pickle(it->second.data(), it->second.size());
      PickleIterator pickit(pickle);
      std::string id;
      while (pickit.ReadString(&id))
        set.insert(id);
    } else {
      cache_->Delete(kComponentNamespaceCache, it->first);
    }
  }
  return components.Pass();
}

ComponentCloudPolicyService::ComponentCloudPolicyService(
    Delegate* delegate,
    CloudPolicyStore* store,
    scoped_ptr<ResourceCache> cache)
    : delegate_(delegate),
      backend_(NULL),
      client_(NULL),
      store_(store),
      is_initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  store_->AddObserver(this);

  // TODO(joaodasilva): this can't currently live on the blocking pool because
  // creating URLFetchers requires a MessageLoop.
  backend_task_runner_ = content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::FILE);
  backend_ = new Backend(weak_ptr_factory_.GetWeakPtr(),
                         backend_task_runner_,
                         cache.Pass());
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Backend::Init, base::Unretained(backend_)));

  if (store_->is_initialized())
    InitializeBackend();
}

ComponentCloudPolicyService::~ComponentCloudPolicyService() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  store_->RemoveObserver(this);
  if (client_)
    client_->RemoveObserver(this);
  backend_task_runner_->DeleteSoon(FROM_HERE, backend_);
  backend_ = NULL;
}

// static
bool ComponentCloudPolicyService::SupportsDomain(PolicyDomain domain) {
  return ComponentCloudPolicyStore::SupportsDomain(domain);
}

void ComponentCloudPolicyService::Connect(
    CloudPolicyClient* client,
    scoped_refptr<net::URLRequestContextGetter> request_context) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!client_);
  client_ = client;
  client_->AddObserver(this);
  // Create the updater in the backend.
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&Backend::Connect,
                                            base::Unretained(backend_),
                                            request_context));
  if (is_initialized())
    InitializeClient();
}

void ComponentCloudPolicyService::Disconnect() {
  if (client_) {
    // Unregister all the current components.
    for (ComponentMap::iterator it = registered_components_.begin();
         it != registered_components_.end(); ++it) {
      RemoveNamespacesToFetch(it->first, it->second);
    }

    client_->RemoveObserver(this);
    client_ = NULL;

    backend_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Backend::Disconnect, base::Unretained(backend_)));
  }
}

void ComponentCloudPolicyService::RegisterPolicyDomain(
    PolicyDomain domain,
    const std::set<std::string>& current_ids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(SupportsDomain(domain));

  // Send the new set to the backend, to purge the cache.
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Backend::SetCurrentComponents,
                 base::Unretained(backend_),
                 domain,
                 base::Owned(new StringSet(current_ids))));

  // Register the current list of components for |domain| at the |client_|.
  StringSet& registered_ids = registered_components_[domain];
  if (client_ && is_initialized()) {
    if (UpdateClientNamespaces(domain, registered_ids, current_ids))
      delegate_->OnComponentCloudPolicyRefreshNeeded();
  }
  registered_ids = current_ids;
}

void ComponentCloudPolicyService::OnPolicyFetched(CloudPolicyClient* client) {
  DCHECK_EQ(client_, client);
  // Pass each PolicyFetchResponse whose policy type is registered to the
  // Backend.
  const CloudPolicyClient::ResponseMap& responses = client_->responses();
  for (CloudPolicyClient::ResponseMap::const_iterator it = responses.begin();
       it != responses.end(); ++it) {
    const PolicyNamespaceKey& key(it->first);
    PolicyDomain domain;
    if (ComponentCloudPolicyStore::GetPolicyDomain(key.first, &domain) &&
        ContainsKey(registered_components_[domain], key.second)) {
      scoped_ptr<em::PolicyFetchResponse> response(
          new em::PolicyFetchResponse(*it->second));
      backend_task_runner_->PostTask(FROM_HERE,
                                     base::Bind(&Backend::UpdateExternalPolicy,
                                                base::Unretained(backend_),
                                                base::Passed(&response)));
    }
  }
}

void ComponentCloudPolicyService::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  // Ignored.
}

void ComponentCloudPolicyService::OnClientError(CloudPolicyClient* client) {
  // Ignored.
}

void ComponentCloudPolicyService::OnStoreLoaded(CloudPolicyStore* store) {
  DCHECK_EQ(store_, store);
  if (store_->is_initialized()) {
    if (is_initialized()) {
      // The backend is already initialized; update the credentials, in case
      // a new dmtoken or server key became available.
      SetCredentialsAndReloadClient();
    } else {
      // The |store_| just became ready; initialize the backend now.
      InitializeBackend();
    }
  }
}

void ComponentCloudPolicyService::OnStoreError(CloudPolicyStore* store) {
  OnStoreLoaded(store);
}

void ComponentCloudPolicyService::InitializeBackend() {
  DCHECK(!is_initialized());
  DCHECK(store_->is_initialized());

  // Set the credentials for the initial policy load, if available.
  SetCredentialsAndReloadClient();

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Backend::FinalizeInit, base::Unretained(backend_)));
}

void ComponentCloudPolicyService::OnBackendInitialized(
    scoped_ptr<ComponentMap> cached_components,
    scoped_ptr<PolicyBundle> initial_policy) {
  // InitializeBackend() may be called multiple times if the |store_| fires
  // events while the backend is loading.
  if (is_initialized())
    return;

  // RegisterPolicyDomain() may have been called while the backend was
  // initializing; only update |registered_components_| from |cached_components|
  // for domains that haven't registered yet.
  for (ComponentMap::iterator it = cached_components->begin();
       it != cached_components->end(); ++it) {
    // Lookup without inserting an empty set.
    if (registered_components_.find(it->first) != registered_components_.end())
      continue;  // Ignore the cached list if a more recent one was registered.
    registered_components_[it->first].swap(it->second);
  }

  // A client may have already connected while the backend was initializing.
  if (client_)
    InitializeClient();

  // Set the initial policy, and send the initial update callback.
  is_initialized_ = true;
  OnPolicyUpdated(initial_policy.Pass());
}

void ComponentCloudPolicyService::InitializeClient() {
  // Register all the current components.
  bool added = false;
  for (ComponentMap::iterator it = registered_components_.begin();
       it != registered_components_.end(); ++it) {
    added |= !it->second.empty();
    AddNamespacesToFetch(it->first, it->second);
  }
  // The client may already have PolicyFetchResponses for registered components;
  // load them now.
  OnPolicyFetched(client_);
  if (added && is_initialized())
    delegate_->OnComponentCloudPolicyRefreshNeeded();
}

void ComponentCloudPolicyService::OnPolicyUpdated(
    scoped_ptr<PolicyBundle> policy) {
  policy_.Swap(policy.get());
  // Don't propagate updates until the initial store Load() has been done.
  if (is_initialized())
    delegate_->OnComponentCloudPolicyUpdated();
}

void ComponentCloudPolicyService::SetCredentialsAndReloadClient() {
  const em::PolicyData* policy = store_->policy();
  if (!policy || !policy->has_username() || !policy->has_request_token())
    return;
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&Backend::SetCredentials,
                                            base::Unretained(backend_),
                                            policy->username(),
                                            policy->request_token()));
  // If this was the initial register, or if the signing key changed, then the
  // previous OnPolicyFetched() call had its PolicyFetchResponses rejected
  // because the credentials weren't updated yet. Reload all the responses in
  // the client now to handle those cases; if those responses have already been
  // validated then they will be ignored.
  if (client_)
    OnPolicyFetched(client_);
}

bool ComponentCloudPolicyService::UpdateClientNamespaces(
    PolicyDomain domain,
    const StringSet& old_set,
    const StringSet& new_set) {
  StringSet added = base::STLSetDifference<StringSet>(new_set, old_set);
  StringSet removed = base::STLSetDifference<StringSet>(old_set, new_set);
  AddNamespacesToFetch(domain, added);
  RemoveNamespacesToFetch(domain, removed);
  return !added.empty();
}

void ComponentCloudPolicyService::AddNamespacesToFetch(PolicyDomain domain,
                                                       const StringSet& set) {
  std::string policy_type;
  if (ComponentCloudPolicyStore::GetPolicyType(domain, &policy_type)) {
    for (StringSet::const_iterator it = set.begin(); it != set.end(); ++it)
      client_->AddNamespaceToFetch(PolicyNamespaceKey(policy_type, *it));
  }
}

void ComponentCloudPolicyService::RemoveNamespacesToFetch(
    PolicyDomain domain,
    const StringSet& set) {
  std::string policy_type;
  if (ComponentCloudPolicyStore::GetPolicyType(domain, &policy_type)) {
    for (StringSet::const_iterator it = set.begin(); it != set.end(); ++it)
      client_->RemoveNamespaceToFetch(PolicyNamespaceKey(policy_type, *it));
  }
}

}  // namespace policy
