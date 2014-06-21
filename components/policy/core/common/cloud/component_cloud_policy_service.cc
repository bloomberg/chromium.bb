// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/component_cloud_policy_service.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/component_cloud_policy_store.h"
#include "components/policy/core/common/cloud/component_cloud_policy_updater.h"
#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

bool NotInSchemaMap(const scoped_refptr<SchemaMap> schema_map,
                    PolicyDomain domain,
                    const std::string& component_id) {
  return schema_map->GetSchema(PolicyNamespace(domain, component_id)) == NULL;
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
  bool initialized_;

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
      store_(this, cache_.get()),
      initialized_(false) {}

ComponentCloudPolicyService::Backend::~Backend() {}

void ComponentCloudPolicyService::Backend::SetCredentials(
    const std::string& username,
    const std::string& dm_token) {
  if (username.empty() || dm_token.empty()) {
    // No sign-in credentials, so drop any cached policy.
    store_.Clear();
  } else {
    store_.SetCredentials(username, dm_token);
  }
}

void ComponentCloudPolicyService::Backend::Init(
    scoped_refptr<SchemaMap> schema_map) {
  DCHECK(!initialized_);

  OnSchemasUpdated(schema_map, scoped_ptr<PolicyNamespaceList>());

  // Read the initial policy. Note that this does not trigger notifications
  // through OnComponentCloudPolicyStoreUpdated. Note also that the cached
  // data may contain names or values that don't match the schema for that
  // component; the data must be cached without modifications so that its
  // integrity can be verified using the hash, but it must also be filtered
  // right after a Load().
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

  initialized_ = true;
}

void ComponentCloudPolicyService::Backend::UpdateExternalPolicy(
    scoped_ptr<em::PolicyFetchResponse> response) {
  updater_->UpdateExternalPolicy(response.Pass());
}

void ComponentCloudPolicyService::Backend::
    OnComponentCloudPolicyStoreUpdated() {
  if (!initialized_) {
    // Ignore notifications triggered by the initial Purge or Clear.
    return;
  }

  scoped_ptr<PolicyBundle> bundle(new PolicyBundle);
  bundle->CopyFrom(store_.policy());
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

  if (removed) {
    for (size_t i = 0; i < removed->size(); ++i)
      updater_->CancelUpdate((*removed)[i]);
  }
}

ComponentCloudPolicyService::ComponentCloudPolicyService(
    Delegate* delegate,
    SchemaRegistry* schema_registry,
    CloudPolicyCore* core,
    scoped_ptr<ResourceCache> cache,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : delegate_(delegate),
      schema_registry_(schema_registry),
      core_(core),
      request_context_(request_context),
      backend_task_runner_(backend_task_runner),
      io_task_runner_(io_task_runner),
      current_schema_map_(new SchemaMap),
      unfiltered_policy_(new PolicyBundle),
      started_loading_initial_policy_(false),
      loaded_initial_policy_(false),
      is_registered_for_cloud_policy_(false),
      weak_ptr_factory_(this) {
  external_policy_data_fetcher_backend_.reset(
      new ExternalPolicyDataFetcherBackend(io_task_runner_, request_context));

  backend_.reset(
      new Backend(weak_ptr_factory_.GetWeakPtr(),
                  backend_task_runner_,
                  base::MessageLoopProxy::current(),
                  cache.Pass(),
                  external_policy_data_fetcher_backend_->CreateFrontend(
                      backend_task_runner_)));

  schema_registry_->AddObserver(this);
  core_->store()->AddObserver(this);

  // Wait for the store and the schema registry to become ready before
  // initializing the backend, so that it can get the initial list of
  // components and the cached credentials (if any) to validate the cached
  // policies.
  if (core_->store()->is_initialized())
    OnStoreLoaded(core_->store());

  // Start observing the core and tracking the state of the client.
  core_->AddObserver(this);
  if (core_->client())
    OnCoreConnected(core_);
}

ComponentCloudPolicyService::~ComponentCloudPolicyService() {
  DCHECK(CalledOnValidThread());

  schema_registry_->RemoveObserver(this);
  core_->store()->RemoveObserver(this);
  core_->RemoveObserver(this);
  if (core_->client())
    OnCoreDisconnecting(core_);

  io_task_runner_->DeleteSoon(FROM_HERE,
                              external_policy_data_fetcher_backend_.release());
  backend_task_runner_->DeleteSoon(FROM_HERE, backend_.release());
}

// static
bool ComponentCloudPolicyService::SupportsDomain(PolicyDomain domain) {
  return ComponentCloudPolicyStore::SupportsDomain(domain);
}

void ComponentCloudPolicyService::ClearCache() {
  DCHECK(CalledOnValidThread());
  // Empty credentials will wipe the cache.
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&Backend::SetCredentials,
                                            base::Unretained(backend_.get()),
                                            std::string(), std::string()));
}

void ComponentCloudPolicyService::OnSchemaRegistryReady() {
  DCHECK(CalledOnValidThread());
  InitializeIfReady();
}

void ComponentCloudPolicyService::OnSchemaRegistryUpdated(
    bool has_new_schemas) {
  DCHECK(CalledOnValidThread());

  // Ignore schema updates until the backend is initialized.
  // OnBackendInitialized() will send the current schema to the backend again,
  // in case it was updated before the backend initialized.
  if (!loaded_initial_policy_)
    return;

  ReloadSchema();

  // Filter the |unfiltered_policy_| again, now that |current_schema_map_| has
  // been updated. We must make sure we never serve invalid policy; we must
  // also filter again if an invalid Schema has now been loaded.
  OnPolicyUpdated(unfiltered_policy_.Pass());
}

void ComponentCloudPolicyService::OnCoreConnected(CloudPolicyCore* core) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(core_, core);

  core_->client()->AddObserver(this);

  // Register the supported policy domains at the client.
  core_->client()->AddNamespaceToFetch(
      PolicyNamespaceKey(dm_protocol::kChromeExtensionPolicyType, ""));

  // Immediately load any PolicyFetchResponses that the client may already
  // have if the backend is ready.
  if (loaded_initial_policy_)
    OnPolicyFetched(core_->client());
}

void ComponentCloudPolicyService::OnCoreDisconnecting(CloudPolicyCore* core) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(core_, core);

  core_->client()->RemoveObserver(this);

  // Remove all the namespaces from the client.
  core_->client()->RemoveNamespaceToFetch(
      PolicyNamespaceKey(dm_protocol::kChromeExtensionPolicyType, ""));
}

void ComponentCloudPolicyService::OnRefreshSchedulerStarted(
    CloudPolicyCore* core) {
  // Ignored.
}

void ComponentCloudPolicyService::OnStoreLoaded(CloudPolicyStore* store) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(core_->store(), store);

  const bool was_registered_before = is_registered_for_cloud_policy_;

  // Send the current credentials to the backend; do this whenever the store
  // updates, to handle the case of the user registering for policy after the
  // session starts, or the user signing out.
  const em::PolicyData* policy = core_->store()->policy();
  std::string username;
  std::string request_token;
  if (policy && policy->has_username() && policy->has_request_token()) {
    is_registered_for_cloud_policy_ = true;
    username = policy->username();
    request_token = policy->request_token();
  } else {
    is_registered_for_cloud_policy_ = false;
  }

  // Empty credentials will wipe the cache.
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&Backend::SetCredentials,
                                            base::Unretained(backend_.get()),
                                            username,
                                            request_token));

  if (!loaded_initial_policy_) {
    // This is the initial load; check if we're ready to initialize the
    // backend, regardless of the signin state.
    InitializeIfReady();
  } else if (!was_registered_before && is_registered_for_cloud_policy_) {
    // We are already initialized, but just sent credentials to the backend for
    // the first time; this means that the user was not registered for cloud
    // policy on startup but registered during the session.
    //
    // When that happens, OnPolicyFetched() is sent to observers before the
    // CloudPolicyStore gets a chance to verify the user policy. In those cases,
    // the backend gets the PolicyFetchResponses before it has the credentials
    // and therefore the validation of those responses fails.
    // Reload any PolicyFetchResponses that the client may have now so that
    // validation is retried with the credentials in place.
    if (core_->client())
      OnPolicyFetched(core_->client());
  }
}

void ComponentCloudPolicyService::OnStoreError(CloudPolicyStore* store) {
  DCHECK(CalledOnValidThread());
  OnStoreLoaded(store);
}

void ComponentCloudPolicyService::OnPolicyFetched(CloudPolicyClient* client) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(core_->client(), client);

  if (!is_registered_for_cloud_policy_) {
    // Trying to load any policies now will fail validation. An OnStoreLoaded()
    // notification should follow soon, after the main user policy has been
    // validated and stored.
    return;
  }

  // Pass each PolicyFetchResponse whose policy type is registered to the
  // Backend.
  const CloudPolicyClient::ResponseMap& responses =
      core_->client()->responses();
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
  // Ignored; the registration state is tracked by looking at the
  // CloudPolicyStore instead.
}

void ComponentCloudPolicyService::OnClientError(CloudPolicyClient* client) {
  DCHECK(CalledOnValidThread());
  // Ignored.
}

void ComponentCloudPolicyService::InitializeIfReady() {
  DCHECK(CalledOnValidThread());
  if (started_loading_initial_policy_ || !schema_registry_->IsReady() ||
      !core_->store()->is_initialized()) {
    return;
  }

  // The initial list of components is ready. Initialize the backend now, which
  // will call back to OnBackendInitialized.
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&Backend::Init,
                                            base::Unretained(backend_.get()),
                                            schema_registry_->schema_map()));
  started_loading_initial_policy_ = true;
}

void ComponentCloudPolicyService::OnBackendInitialized(
    scoped_ptr<PolicyBundle> initial_policy) {
  DCHECK(CalledOnValidThread());
  DCHECK(!loaded_initial_policy_);

  loaded_initial_policy_ = true;

  // Send the current schema to the backend, in case it has changed while the
  // backend was initializing.
  ReloadSchema();

  // We're now ready to serve the initial policy; notify the policy observers.
  OnPolicyUpdated(initial_policy.Pass());
}

void ComponentCloudPolicyService::ReloadSchema() {
  DCHECK(CalledOnValidThread());

  scoped_ptr<PolicyNamespaceList> removed(new PolicyNamespaceList);
  PolicyNamespaceList added;
  const scoped_refptr<SchemaMap>& new_schema_map =
      schema_registry_->schema_map();
  new_schema_map->GetChanges(current_schema_map_, removed.get(), &added);

  current_schema_map_ = new_schema_map;

  // Send the updated SchemaMap and a list of removed components to the
  // backend.
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&Backend::OnSchemasUpdated,
                                            base::Unretained(backend_.get()),
                                            current_schema_map_,
                                            base::Passed(&removed)));

  // Have another look at the client if the core is already connected.
  // The client may have already fetched policy for some component and it was
  // previously ignored because the component wasn't listed in the schema map.
  // There's no point in fetching policy from the server again; the server
  // always pushes all the components it knows about.
  if (core_->client())
    OnPolicyFetched(core_->client());
}

void ComponentCloudPolicyService::OnPolicyUpdated(
    scoped_ptr<PolicyBundle> policy) {
  DCHECK(CalledOnValidThread());

  // Store the current unfiltered policies.
  unfiltered_policy_ = policy.Pass();

  // Make a copy in |policy_| and filter it; this is what's passed to the
  // outside world.
  policy_.CopyFrom(*unfiltered_policy_);
  current_schema_map_->FilterBundle(&policy_);

  delegate_->OnComponentCloudPolicyUpdated();
}

}  // namespace policy
