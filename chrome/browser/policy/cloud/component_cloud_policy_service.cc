// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/component_cloud_policy_service.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/component_cloud_policy_store.h"
#include "chrome/browser/policy/cloud/component_cloud_policy_updater.h"
#include "chrome/browser/policy/cloud/external_policy_data_fetcher.h"
#include "chrome/browser/policy/cloud/resource_cache.h"
#include "chrome/browser/policy/proto/cloud/device_management_backend.pb.h"
#include "chrome/browser/policy/schema_map.h"
#include "components/policy/core/common/schema.h"
#include "net/url_request/url_request_context_getter.h"

namespace em = enterprise_management;

namespace policy {

namespace {

bool NotInSchemaMap(const scoped_refptr<SchemaMap> schema_map,
                    PolicyDomain domain,
                    const std::string& component_id) {
  return schema_map->GetSchema(PolicyNamespace(domain, component_id)) == NULL;
}

bool ToPolicyNamespaceKey(const PolicyNamespace& ns, PolicyNamespaceKey* key) {
  if (!ComponentCloudPolicyStore::GetPolicyType(ns.domain, &key->first))
    return false;
  key->second = ns.component_id;
  return true;
}

bool ToPolicyNamespace(const PolicyNamespaceKey& key, PolicyNamespace* ns) {
  if (!ComponentCloudPolicyStore::GetPolicyDomain(key.first, &ns->domain))
    return false;
  ns->component_id = key.second;
  return true;
}

}  // namespace

ComponentCloudPolicyService::Delegate::~Delegate() {}

// Owns the objects that live on the background thread, and posts back to the
// thread that the ComponentCloudPolicyService runs on whenever the policy
// changes.
class ComponentCloudPolicyService::Backend
    : public ComponentCloudPolicyStore::Delegate {
 public:
  // This class can be instantiated on any thread but from then on, may be
  // accessed via the |task_runner_| only. Policy changes are posted to the
  // |service| via the |service_task_runner|. The |cache| is used to load and
  // store local copies of the downloaded policies.
  Backend(base::WeakPtr<ComponentCloudPolicyService> service,
          scoped_refptr<base::SequencedTaskRunner> task_runner,
          scoped_refptr<base::SequencedTaskRunner> service_task_runner,
          scoped_ptr<ResourceCache> cache,
          scoped_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher);

  virtual ~Backend();

  // |username| and |dm_token| will be  used to validate the cached policies.
  void SetCredentials(const std::string& username, const std::string& dm_token);

  // Loads the |store_| and starts downloading updates.
  void Init(scoped_refptr<SchemaMap> schema_map);

  // Passes a policy protobuf to the backend, to start its validation and
  // eventual download of the policy data on the background thread.
  void UpdateExternalPolicy(scoped_ptr<em::PolicyFetchResponse> response);

  // ComponentCloudPolicyStore::Delegate implementation:
  virtual void OnComponentCloudPolicyStoreUpdated() OVERRIDE;

  // Passes the current SchemaMap so that the disk cache can purge components
  // that aren't being tracked anymore.
  // |removed| is a list of namespaces that were present in the previous
  // schema and have been removed in the updated version.
  void OnSchemasUpdated(scoped_refptr<SchemaMap> schema_map,
                        scoped_ptr<PolicyNamespaceList> removed);

 private:
  // The ComponentCloudPolicyService that owns |this|. Used to inform the
  // |service_| when policy changes.
  base::WeakPtr<ComponentCloudPolicyService> service_;

  // The thread that |this| runs on. Used to post tasks to be run by |this|.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The thread that the |service_| runs on. Used to post policy changes to the
  // right thread.
  scoped_refptr<base::SequencedTaskRunner> service_task_runner_;

  scoped_ptr<ResourceCache> cache_;
  scoped_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher_;
  ComponentCloudPolicyStore store_;
  scoped_ptr<ComponentCloudPolicyUpdater> updater_;
  scoped_refptr<SchemaMap> schema_map_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

ComponentCloudPolicyService::Backend::Backend(
    base::WeakPtr<ComponentCloudPolicyService> service,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> service_task_runner,
    scoped_ptr<ResourceCache> cache,
    scoped_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher)
    : service_(service),
      task_runner_(task_runner),
      service_task_runner_(service_task_runner),
      cache_(cache.Pass()),
      external_policy_data_fetcher_(external_policy_data_fetcher.Pass()),
      store_(this, cache_.get()) {}

ComponentCloudPolicyService::Backend::~Backend() {}

void ComponentCloudPolicyService::Backend::SetCredentials(
    const std::string& username,
    const std::string& dm_token) {
  store_.SetCredentials(username, dm_token);
}

void ComponentCloudPolicyService::Backend::Init(
    scoped_refptr<SchemaMap> schema_map) {
  DCHECK(!schema_map_);

  OnSchemasUpdated(schema_map, scoped_ptr<PolicyNamespaceList>());

  // Read the initial policy. Note that this does not trigger notifications
  // through OnComponentCloudPolicyStoreUpdated.
  store_.Load();
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle);
  bundle->CopyFrom(store_.policy());

  // Start downloading any pending data.
  updater_.reset(new ComponentCloudPolicyUpdater(
      task_runner_, external_policy_data_fetcher_.Pass(), &store_));

  service_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ComponentCloudPolicyService::OnBackendInitialized,
                 service_,
                 base::Passed(&bundle)));
}

void ComponentCloudPolicyService::Backend::UpdateExternalPolicy(
    scoped_ptr<em::PolicyFetchResponse> response) {
  updater_->UpdateExternalPolicy(response.Pass());
}

void ComponentCloudPolicyService::Backend::
    OnComponentCloudPolicyStoreUpdated() {
  if (!schema_map_) {
    // Ignore notifications triggered by the initial Purge.
    return;
  }

  scoped_ptr<PolicyBundle> bundle(new PolicyBundle);
  bundle->CopyFrom(store_.policy());
  schema_map_->FilterBundle(bundle.get());
  service_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ComponentCloudPolicyService::OnPolicyUpdated,
                 service_,
                 base::Passed(&bundle)));
}

void ComponentCloudPolicyService::Backend::OnSchemasUpdated(
    scoped_refptr<SchemaMap> schema_map,
    scoped_ptr<PolicyNamespaceList> removed) {
  // Purge any components that have been removed.
  const DomainMap& domains = schema_map->GetDomains();
  for (DomainMap::const_iterator domain = domains.begin();
       domain != domains.end(); ++domain) {
    store_.Purge(domain->first,
                 base::Bind(&NotInSchemaMap, schema_map, domain->first));
  }

  // Set |schema_map_| after purging so that the notifications from the store
  // are ignored on the first OnSchemasUpdated() call from Init().
  schema_map_ = schema_map;

  if (removed) {
    for (size_t i = 0; i < removed->size(); ++i)
      updater_->CancelUpdate((*removed)[i]);
  }
}

ComponentCloudPolicyService::ComponentCloudPolicyService(
    Delegate* delegate,
    SchemaRegistry* schema_registry,
    CloudPolicyStore* store,
    scoped_ptr<ResourceCache> cache,
    CloudPolicyClient* client,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : delegate_(delegate),
      schema_registry_(schema_registry),
      store_(store),
      client_(client),
      request_context_(request_context),
      backend_task_runner_(backend_task_runner),
      io_task_runner_(io_task_runner),
      current_schema_map_(new SchemaMap),
      is_initialized_(false),
      has_credentials_(false),
      weak_ptr_factory_(this) {
  schema_registry_->AddObserver(this);
  store_->AddObserver(this);
  client_->AddObserver(this);

  external_policy_data_fetcher_backend_.reset(
      new ExternalPolicyDataFetcherBackend(io_task_runner_, request_context));

  backend_.reset(
      new Backend(weak_ptr_factory_.GetWeakPtr(),
                  backend_task_runner_,
                  base::MessageLoopProxy::current(),
                  cache.Pass(),
                  external_policy_data_fetcher_backend_->CreateFrontend(
                      backend_task_runner_)));

  if (store_->is_initialized())
    OnStoreLoaded(store_);
}

ComponentCloudPolicyService::~ComponentCloudPolicyService() {
  DCHECK(CalledOnValidThread());
  schema_registry_->RemoveObserver(this);
  store_->RemoveObserver(this);
  client_->RemoveObserver(this);

  // Remove all the namespaces from |client_| but don't send this empty schema
  // to the backend, to avoid dropping the caches.
  if (is_initialized()) {
    scoped_refptr<SchemaMap> empty(new SchemaMap);
    SetCurrentSchema(empty, false);
  }

  io_task_runner_->DeleteSoon(FROM_HERE,
                              external_policy_data_fetcher_backend_.release());
  backend_task_runner_->DeleteSoon(FROM_HERE, backend_.release());
}

// static
bool ComponentCloudPolicyService::SupportsDomain(PolicyDomain domain) {
  return ComponentCloudPolicyStore::SupportsDomain(domain);
}

void ComponentCloudPolicyService::OnPolicyFetched(CloudPolicyClient* client) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(client_, client);

  if (!is_initialized() || !has_credentials_)
    return;

  // Pass each PolicyFetchResponse whose policy type is registered to the
  // Backend.
  const CloudPolicyClient::ResponseMap& responses = client_->responses();
  for (CloudPolicyClient::ResponseMap::const_iterator it = responses.begin();
       it != responses.end(); ++it) {
    PolicyNamespace ns;
    if (ToPolicyNamespace(it->first, &ns) &&
        current_schema_map_->GetSchema(ns)) {
      scoped_ptr<em::PolicyFetchResponse> response(
          new em::PolicyFetchResponse(*it->second));
      backend_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&Backend::UpdateExternalPolicy,
                     base::Unretained(backend_.get()),
                     base::Passed(&response)));
    }
  }
}

void ComponentCloudPolicyService::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DCHECK(CalledOnValidThread());
  // Ignored.
}

void ComponentCloudPolicyService::OnClientError(CloudPolicyClient* client) {
  DCHECK(CalledOnValidThread());
  // Ignored.
}

void ComponentCloudPolicyService::OnStoreLoaded(CloudPolicyStore* store) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(store_, store);

  if (!store_->is_initialized())
    return;

  const em::PolicyData* policy = store_->policy();
  if (!has_credentials_ && policy && policy->has_username() &&
      policy->has_request_token()) {
    // Send the current credentials to the backend, if they haven't been sent
    // before. Usually this happens at startup if the user already had a cached
    // cloud policy; otherwise it happens right after the initial registration
    // for cloud policy.
    backend_task_runner_->PostTask(FROM_HERE,
                                   base::Bind(&Backend::SetCredentials,
                                              base::Unretained(backend_.get()),
                                              policy->username(),
                                              policy->request_token()));
    has_credentials_ = true;
    if (is_initialized()) {
      // This was the first policy fetch for this client. Process any
      // PolicyFetchResponses that the client may have now; processing them
      // before the credentials were sent to the backend would fail validation.
      OnPolicyFetched(client_);
    }
  }

  if (!is_initialized())
    InitializeIfReady();
}

void ComponentCloudPolicyService::OnStoreError(CloudPolicyStore* store) {
  DCHECK(CalledOnValidThread());
  OnStoreLoaded(store);
}

void ComponentCloudPolicyService::OnSchemaRegistryReady() {
  DCHECK(CalledOnValidThread());
  InitializeIfReady();
}

void ComponentCloudPolicyService::OnSchemaRegistryUpdated(
    bool has_new_schemas) {
  DCHECK(CalledOnValidThread());

  if (!is_initialized())
    return;

  // When an extension is reloaded or updated, it triggers an unregister quickly
  // followed by a register in the SchemaRegistry. If the intermediate version
  // of the SchemaMap is passed to the backend then it will drop the cached
  // policy for that extension and will trigger a new policy fetch soon after.
  // Delaying the schema update here coalesces both updates into one, and the
  // new schema will equal the older version in case of extension updates.
  //
  // TODO(joaodasilva): Increase this delay to 10 seconds. For now it's
  // immediate so that tests don't get delayed.
  schema_update_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(0),
      base::Bind(&ComponentCloudPolicyService::SetCurrentSchema,
                 base::Unretained(this),
                 schema_registry_->schema_map(),
                 true));
}

void ComponentCloudPolicyService::InitializeIfReady() {
  DCHECK(CalledOnValidThread());
  if (!schema_registry_->IsReady() || !store_->is_initialized())
    return;
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&Backend::Init,
                                            base::Unretained(backend_.get()),
                                            schema_registry_->schema_map()));
}

void ComponentCloudPolicyService::OnBackendInitialized(
    scoped_ptr<PolicyBundle> initial_policy) {
  DCHECK(CalledOnValidThread());

  is_initialized_ = true;

  // Send the current schema to the backend, in case it has changed while the
  // backend was initializing.
  SetCurrentSchema(schema_registry_->schema_map(), true);

  // Process any PolicyFetchResponses that the client may already have, or that
  // may have been received while the backend was initializing.
  OnPolicyFetched(client_);

  // Finally tell the Delegate that the initial policy is available.
  OnPolicyUpdated(initial_policy.Pass());
}

void ComponentCloudPolicyService::SetCurrentSchema(
    const scoped_refptr<SchemaMap>& new_schema_map,
    bool send_to_backend) {
  DCHECK(CalledOnValidThread());
  DCHECK(is_initialized());

  scoped_ptr<PolicyNamespaceList> removed(new PolicyNamespaceList);
  PolicyNamespaceList added;
  new_schema_map->GetChanges(current_schema_map_, removed.get(), &added);

  current_schema_map_ = new_schema_map;

  for (size_t i = 0; i < removed->size(); ++i) {
    PolicyNamespaceKey key;
    if (ToPolicyNamespaceKey((*removed)[i], &key))
      client_->RemoveNamespaceToFetch(key);
  }

  bool added_namespaces_to_client = false;
  for (size_t i = 0; i < added.size(); ++i) {
    PolicyNamespaceKey key;
    if (ToPolicyNamespaceKey(added[i], &key)) {
      client_->AddNamespaceToFetch(key);
      added_namespaces_to_client = true;
    }
  }

  if (added_namespaces_to_client)
    delegate_->OnComponentCloudPolicyRefreshNeeded();

  if (send_to_backend) {
    backend_task_runner_->PostTask(FROM_HERE,
                                   base::Bind(&Backend::OnSchemasUpdated,
                                              base::Unretained(backend_.get()),
                                              current_schema_map_,
                                              base::Passed(&removed)));
  }
}

void ComponentCloudPolicyService::OnPolicyUpdated(
    scoped_ptr<PolicyBundle> policy) {
  DCHECK(CalledOnValidThread());
  policy_.Swap(policy.get());
  delegate_->OnComponentCloudPolicyUpdated();
}

}  // namespace policy
