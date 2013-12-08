// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_store.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"
#include "components/policy/core/common/cloud/external_policy_data_updater.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "net/url_request/url_request_context_getter.h"

namespace policy {

namespace {

// Fetch data for at most two external data references at the same time.
const int kMaxParallelFetches = 2;

// Allows policies to reference |max_external_data_size_for_testing| bytes of
// external data even if no |max_size| was specified in policy_templates.json.
int max_external_data_size_for_testing = 0;

}  // namespace

// Backend for the CloudExternalDataManagerBase that handles all data download,
// verification, caching and retrieval.
class CloudExternalDataManagerBase::Backend {
 public:
  // |get_policy_details| is used to determine the maximum size that the
  // data referenced by each policy can have. This class can be instantiated on
  // any thread but from then on, may be accessed via the |task_runner_| only.
  // All FetchCallbacks will be invoked via |callback_task_runner|.
  Backend(const GetChromePolicyDetailsCallback& get_policy_details,
          scoped_refptr<base::SequencedTaskRunner> task_runner,
          scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

  // Allows downloaded external data to be cached in |external_data_store|.
  // Ownership of the store is taken. The store can be destroyed by calling
  // SetExternalDataStore(scoped_ptr<CloudExternalDataStore>()).
  void SetExternalDataStore(
      scoped_ptr<CloudExternalDataStore> external_data_store);

  // Allows downloading of external data via the |external_policy_data_fetcher|.
  void Connect(
      scoped_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher);

  // Prevents further external data downloads and aborts any downloads currently
  // in progress
  void Disconnect();

  // Called when the external data references that this backend is responsible
  // for change. |metadata| maps from policy names to the metadata specifying
  // the external data that each of the policies references.
  void OnMetadataUpdated(scoped_ptr<Metadata> metadata);

  // Called by the |updater_| when the external |data| referenced by |policy|
  // has been successfully downloaded and verified to match |hash|.
  bool OnDownloadSuccess(const std::string& policy,
                         const std::string& hash,
                         const std::string& data);

  // Retrieves the external data referenced by |policy| and invokes |callback|
  // with the result. If |policy| does not reference any external data, the
  // |callback| is invoked with a NULL pointer. Otherwise, the |callback| is
  // invoked with the referenced data once it has been successfully retrieved.
  // If retrieval is temporarily impossible (e.g. the data is not cached yet and
  // there is no network connectivity), the |callback| will be invoked when the
  // temporary hindrance is resolved. If retrieval is permanently impossible
  // (e.g. |policy| references data that does not exist on the server), the
  // |callback| will never be invoked.
  // If the data for |policy| is not cached yet, only one download is started,
  // even if this method is invoked multiple times. The |callback|s passed are
  // enqueued and all invoked once the data has been successfully retrieved.
  void Fetch(const std::string& policy,
             const ExternalDataFetcher::FetchCallback& callback);

  // Try to download and cache all external data referenced by |metadata_|.
  void FetchAll();

 private:
  // List of callbacks to invoke when the attempt to retrieve external data
  // referenced by a policy completes successfully or fails permanently.
  typedef std::vector<ExternalDataFetcher::FetchCallback> FetchCallbackList;

  // Map from policy names to the lists of callbacks defined above.
  typedef std::map<std::string, FetchCallbackList> FetchCallbackMap;

  // Looks up the maximum size that the data referenced by |policy| can have.
  size_t GetMaxExternalDataSize(const std::string& policy) const;

  // Invokes |callback| via the |callback_task_runner_|, passing |data| as a
  // parameter.
  void RunCallback(const ExternalDataFetcher::FetchCallback& callback,
                   scoped_ptr<std::string> data) const;

  // Tells the |updater_| to download the external data referenced by |policy|.
  // If Connect() was not called yet and no |updater_| exists, does nothing.
  void StartDownload(const std::string& policy);

  // Used to determine the maximum size that the data referenced by each policy
  // can have.
  GetChromePolicyDetailsCallback get_policy_details_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  // Contains the policies for which a download of the referenced external data
  // has been requested. Each policy is mapped to a list of callbacks to invoke
  // when the download completes successfully or fails permanently. If no
  // callback needs to be invoked (because the download was requested via
  // FetchAll()), a map entry will still exist but the list of callbacks it maps
  // to will be empty.
  FetchCallbackMap pending_downloads_;

  // Indicates that OnMetadataUpdated() has been called at least once and the
  // contents of |metadata_| is initialized.
  bool metadata_set_;

  // Maps from policy names to the metadata specifying the external data that
  // each of the policies references.
  Metadata metadata_;

  // Used to cache external data referenced by policies.
  scoped_ptr<CloudExternalDataStore> external_data_store_;

  // Used to download external data referenced by policies.
  scoped_ptr<ExternalPolicyDataUpdater> updater_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

CloudExternalDataManagerBase::Backend::Backend(
    const GetChromePolicyDetailsCallback& get_policy_details,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
    : get_policy_details_(get_policy_details),
      task_runner_(task_runner),
      callback_task_runner_(callback_task_runner),
      metadata_set_(false) {
}

void CloudExternalDataManagerBase::Backend::SetExternalDataStore(
    scoped_ptr<CloudExternalDataStore> external_data_store) {
  external_data_store_.reset(external_data_store.release());
  if (metadata_set_ && external_data_store_)
    external_data_store_->Prune(metadata_);
}

void CloudExternalDataManagerBase::Backend::Connect(
    scoped_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher) {
  DCHECK(!updater_);
  updater_.reset(new ExternalPolicyDataUpdater(
      task_runner_,
      external_policy_data_fetcher.Pass(),
      kMaxParallelFetches));
  for (FetchCallbackMap::const_iterator it = pending_downloads_.begin();
       it != pending_downloads_.end(); ++it) {
    StartDownload(it->first);
  }
}

void CloudExternalDataManagerBase::Backend::Disconnect() {
  updater_.reset();
}

void CloudExternalDataManagerBase::Backend::OnMetadataUpdated(
    scoped_ptr<Metadata> metadata) {
  metadata_set_ = true;
  Metadata old_metadata;
  metadata_.swap(old_metadata);
  if (metadata)
    metadata_.swap(*metadata);

  if (external_data_store_)
    external_data_store_->Prune(metadata_);

  for (FetchCallbackMap::iterator it = pending_downloads_.begin();
       it != pending_downloads_.end(); ) {
    const std::string policy = it->first;
    Metadata::const_iterator metadata = metadata_.find(policy);
    if (metadata == metadata_.end()) {
      // |policy| no longer references external data.
      if (updater_) {
        // Cancel the external data download.
        updater_->CancelExternalDataFetch(policy);
      }
      for (FetchCallbackList::const_iterator callback = it->second.begin();
           callback != it->second.end(); ++callback) {
        // Invoke all callbacks for |policy|, indicating permanent failure.
        RunCallback(*callback, scoped_ptr<std::string>());
      }
      pending_downloads_.erase(it++);
      continue;
    }

    if (updater_ && metadata->second != old_metadata[policy]) {
      // |policy| still references external data but the reference has changed.
      // Cancel the external data download and start a new one.
      updater_->CancelExternalDataFetch(policy);
      StartDownload(policy);
    }
    ++it;
  }
}

bool CloudExternalDataManagerBase::Backend::OnDownloadSuccess(
    const std::string& policy,
    const std::string& hash,
    const std::string& data) {
  DCHECK(metadata_.find(policy) != metadata_.end());
  DCHECK_EQ(hash, metadata_[policy].hash);
  if (external_data_store_)
    external_data_store_->Store(policy, hash, data);

  const FetchCallbackList& pending_callbacks = pending_downloads_[policy];
  for (FetchCallbackList::const_iterator it = pending_callbacks.begin();
       it != pending_callbacks.end(); ++it) {
    RunCallback(*it, make_scoped_ptr(new std::string(data)));
  }
  pending_downloads_.erase(policy);
  return true;
}

void CloudExternalDataManagerBase::Backend::Fetch(
    const std::string& policy,
    const ExternalDataFetcher::FetchCallback& callback) {
  Metadata::const_iterator metadata = metadata_.find(policy);
  if (metadata == metadata_.end()) {
    // If |policy| does not reference any external data, indicate permanent
    // failure.
    RunCallback(callback, scoped_ptr<std::string>());
    return;
  }

  if (pending_downloads_.find(policy) != pending_downloads_.end()) {
    // If a download of the external data referenced by |policy| has already
    // been requested, add |callback| to the list of callbacks for |policy| and
    // return.
    pending_downloads_[policy].push_back(callback);
    return;
  }

  scoped_ptr<std::string> data(new std::string);
  if (external_data_store_ && external_data_store_->Load(
          policy, metadata->second.hash, GetMaxExternalDataSize(policy),
          data.get())) {
    // If the external data referenced by |policy| exists in the cache and
    // matches the expected hash, pass it to the callback.
    RunCallback(callback, data.Pass());
    return;
  }

  // Request a download of the the external data referenced by |policy| and
  // initialize the list of callbacks by adding |callback|.
  pending_downloads_[policy].push_back(callback);
  StartDownload(policy);
}

void CloudExternalDataManagerBase::Backend::FetchAll() {
  // Loop through all external data references.
  for (Metadata::const_iterator it = metadata_.begin(); it != metadata_.end();
       ++it) {
    const std::string& policy = it->first;
    scoped_ptr<std::string> data(new std::string);
    if (pending_downloads_.find(policy) != pending_downloads_.end() ||
        (external_data_store_ && external_data_store_->Load(
             policy, it->second.hash, GetMaxExternalDataSize(policy),
             data.get()))) {
      // If a download of the external data referenced by |policy| has already
      // been requested or the data exists in the cache and matches the expected
      // hash, there is nothing to be done.
      continue;
    }
    // Request a download of the the external data referenced by |policy| and
    // initialize the list of callbacks to an empty list.
    pending_downloads_[policy];
    StartDownload(policy);
  }
}

size_t CloudExternalDataManagerBase::Backend::GetMaxExternalDataSize(
    const std::string& policy) const {
  if (max_external_data_size_for_testing)
    return max_external_data_size_for_testing;

  // Look up the maximum size that the data referenced by |policy| can have in
  // get_policy_details, which is constructed from the information in
  // policy_templates.json, allowing the maximum data size to be specified as
  // part of the policy definition.
  const PolicyDetails* details = get_policy_details_.Run(policy);
  if (details)
    return details->max_external_data_size;
  NOTREACHED();
  return 0;
}

void CloudExternalDataManagerBase::Backend::RunCallback(
    const ExternalDataFetcher::FetchCallback& callback,
    scoped_ptr<std::string> data) const {
  callback_task_runner_->PostTask(FROM_HERE,
                                  base::Bind(callback, base::Passed(&data)));
}

void CloudExternalDataManagerBase::Backend::StartDownload(
    const std::string& policy) {
  DCHECK(pending_downloads_.find(policy) != pending_downloads_.end());
  if (!updater_)
    return;

  const MetadataEntry& metadata = metadata_[policy];
  updater_->FetchExternalData(
      policy,
      ExternalPolicyDataUpdater::Request(metadata.url,
                                         metadata.hash,
                                         GetMaxExternalDataSize(policy)),
      base::Bind(&CloudExternalDataManagerBase::Backend::OnDownloadSuccess,
                 base::Unretained(this),
                 policy,
                 metadata.hash));
}

CloudExternalDataManagerBase::CloudExternalDataManagerBase(
    const GetChromePolicyDetailsCallback& get_policy_details,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : backend_task_runner_(backend_task_runner),
      io_task_runner_(io_task_runner),
      backend_(new Backend(get_policy_details,
                           backend_task_runner_,
                           base::MessageLoopProxy::current())) {
}

CloudExternalDataManagerBase::~CloudExternalDataManagerBase() {
  DCHECK(CalledOnValidThread());
  io_task_runner_->DeleteSoon(FROM_HERE,
                              external_policy_data_fetcher_backend_.release());
  backend_task_runner_->DeleteSoon(FROM_HERE, backend_.release());
}

void CloudExternalDataManagerBase::SetExternalDataStore(
    scoped_ptr<CloudExternalDataStore> external_data_store) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(FROM_HERE, base::Bind(
      &Backend::SetExternalDataStore,
      base::Unretained(backend_.get()),
      base::Passed(&external_data_store)));
}

void CloudExternalDataManagerBase::SetPolicyStore(
    CloudPolicyStore* policy_store) {
  DCHECK(CalledOnValidThread());
  CloudExternalDataManager::SetPolicyStore(policy_store);
  if (policy_store_ && policy_store_->is_initialized())
    OnPolicyStoreLoaded();
}

void CloudExternalDataManagerBase::OnPolicyStoreLoaded() {
  // Collect all external data references made by policies in |policy_store_|
  // and pass them to the |backend_|.
  DCHECK(CalledOnValidThread());
  scoped_ptr<Metadata> metadata(new Metadata);
  const PolicyMap& policy_map = policy_store_->policy_map();
  for (PolicyMap::const_iterator it = policy_map.begin();
       it != policy_map.end(); ++it) {
    if (!it->second.external_data_fetcher) {
      // Skip policies that do not reference external data.
      continue;
    }
    const base::DictionaryValue* dict = NULL;
    std::string url;
    std::string hex_hash;
    std::vector<uint8> hash;
    if (it->second.value && it->second.value->GetAsDictionary(&dict) &&
        dict->GetStringWithoutPathExpansion("url", &url) &&
        dict->GetStringWithoutPathExpansion("hash", &hex_hash) &&
        !url.empty() && !hex_hash.empty() &&
        base::HexStringToBytes(hex_hash, &hash)) {
      // Add the external data reference to |metadata| if it is valid (URL and
      // hash are not empty, hash can be decoded as a hex string).
      (*metadata)[it->first] =
          MetadataEntry(url, std::string(hash.begin(), hash.end()));
    }
  }

  backend_task_runner_->PostTask(FROM_HERE, base::Bind(
      &Backend::OnMetadataUpdated,
      base::Unretained(backend_.get()),
      base::Passed(&metadata)));
}

void CloudExternalDataManagerBase::Connect(
    scoped_refptr<net::URLRequestContextGetter> request_context) {
  DCHECK(CalledOnValidThread());
  DCHECK(!external_policy_data_fetcher_backend_);
  external_policy_data_fetcher_backend_.reset(
      new ExternalPolicyDataFetcherBackend(io_task_runner_,
                                           request_context));
  backend_task_runner_->PostTask(FROM_HERE, base::Bind(
      &Backend::Connect,
      base::Unretained(backend_.get()),
      base::Passed(external_policy_data_fetcher_backend_->CreateFrontend(
          backend_task_runner_))));
}

void CloudExternalDataManagerBase::Disconnect() {
  DCHECK(CalledOnValidThread());
  io_task_runner_->DeleteSoon(FROM_HERE,
                              external_policy_data_fetcher_backend_.release());
  backend_task_runner_->PostTask(FROM_HERE, base::Bind(
      &Backend::Disconnect, base::Unretained(backend_.get())));
}

void CloudExternalDataManagerBase::Fetch(
    const std::string& policy,
    const ExternalDataFetcher::FetchCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(FROM_HERE, base::Bind(
      &Backend::Fetch, base::Unretained(backend_.get()), policy, callback));
}

// static
void CloudExternalDataManagerBase::SetMaxExternalDataSizeForTesting(
    int max_size) {
  max_external_data_size_for_testing = max_size;
}

void CloudExternalDataManagerBase::FetchAll() {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(FROM_HERE, base::Bind(
      &Backend::FetchAll, base::Unretained(backend_.get())));
}

}  // namespace policy
