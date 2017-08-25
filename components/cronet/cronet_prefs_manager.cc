// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/cronet_prefs_manager.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/cronet/host_cache_persistence_manager.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "net/http/http_server_properties_manager.h"
#include "net/nqe/network_qualities_prefs_manager.h"
#include "net/sdch/sdch_owner.h"
#include "net/url_request/url_request_context_builder.h"

namespace cronet {
namespace {

// Name of the pref used for HTTP server properties persistence.
const char kHttpServerPropertiesPref[] = "net.http_server_properties";
// Name of preference directory.
const char kPrefsDirectoryName[] = "prefs";
// Name of preference file.
const char kPrefsFileName[] = "local_prefs.json";
// Current version of disk storage.
const int32_t kStorageVersion = 1;
// Version number used when the version of disk storage is unknown.
const uint32_t kStorageVersionUnknown = 0;
// Name of the pref used for host cache persistence.
const char kHostCachePref[] = "net.host_cache";
// Name of the pref used for NQE persistence.
const char kNetworkQualitiesPref[] = "net.network_qualities";

bool IsCurrentVersion(const base::FilePath& version_filepath) {
  if (!base::PathExists(version_filepath))
    return false;
  base::File version_file(version_filepath,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
  uint32_t version = kStorageVersionUnknown;
  int bytes_read =
      version_file.Read(0, reinterpret_cast<char*>(&version), sizeof(version));
  if (bytes_read != sizeof(version)) {
    DLOG(WARNING) << "Cannot read from version file.";
    return false;
  }
  return version == kStorageVersion;
}

// TODO(xunjieli): Handle failures.
void InitializeStorageDirectory(const base::FilePath& dir) {
  // Checks version file and clear old storage.
  base::FilePath version_filepath = dir.Append("version");
  if (IsCurrentVersion(version_filepath)) {
    // The version is up to date, so there is nothing to do.
    return;
  }
  // Delete old directory recursively and create a new directory.
  // base::DeleteFile returns true if the directory does not exist, so it is
  // fine if there is nothing on disk.
  if (!(base::DeleteFile(dir, true) && base::CreateDirectory(dir))) {
    DLOG(WARNING) << "Cannot purge directory.";
    return;
  }
  base::File new_version_file(version_filepath, base::File::FLAG_CREATE_ALWAYS |
                                                    base::File::FLAG_WRITE);

  if (!new_version_file.IsValid()) {
    DLOG(WARNING) << "Cannot create a version file.";
    return;
  }

  DCHECK(new_version_file.created());
  uint32_t new_version = kStorageVersion;
  int bytes_written = new_version_file.Write(
      0, reinterpret_cast<char*>(&new_version), sizeof(new_version));
  if (bytes_written != sizeof(new_version)) {
    DLOG(WARNING) << "Cannot write to version file.";
    return;
  }
  base::FilePath prefs_dir = dir.Append(FILE_PATH_LITERAL(kPrefsDirectoryName));
  if (!base::CreateDirectory(prefs_dir)) {
    DLOG(WARNING) << "Cannot create prefs directory";
    return;
  }
}

// Connects the HttpServerPropertiesManager's storage to the prefs.
class PrefServiceAdapter
    : public net::HttpServerPropertiesManager::PrefDelegate {
 public:
  explicit PrefServiceAdapter(PrefService* pref_service)
      : pref_service_(pref_service), path_(kHttpServerPropertiesPref) {
    pref_change_registrar_.Init(pref_service_);
  }

  ~PrefServiceAdapter() override {}

  // PrefDelegate implementation.
  bool HasServerProperties() override {
    return pref_service_->HasPrefPath(path_);
  }

  const base::DictionaryValue& GetServerProperties() const override {
    // Guaranteed not to return null when the pref is registered
    // (RegisterProfilePrefs was called).
    return *pref_service_->GetDictionary(path_);
  }

  void SetServerProperties(const base::DictionaryValue& value) override {
    return pref_service_->Set(path_, value);
  }

  void StartListeningForUpdates(const base::Closure& callback) override {
    pref_change_registrar_.Add(path_, callback);
  }

  void StopListeningForUpdates() override {
    pref_change_registrar_.RemoveAll();
  }

 private:
  PrefService* pref_service_;
  const std::string path_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceAdapter);
};  // class PrefServiceAdapter

class NetworkQualitiesPrefDelegateImpl
    : public net::NetworkQualitiesPrefsManager::PrefDelegate {
 public:
  // Caller must guarantee that |pref_service| outlives |this|.
  explicit NetworkQualitiesPrefDelegateImpl(PrefService* pref_service)
      : pref_service_(pref_service),
        lossy_prefs_writing_task_posted_(false),
        weak_ptr_factory_(this) {
    DCHECK(pref_service_);
  }

  ~NetworkQualitiesPrefDelegateImpl() override {}

  // net::NetworkQualitiesPrefsManager::PrefDelegate implementation.
  void SetDictionaryValue(const base::DictionaryValue& value) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    pref_service_->Set(kNetworkQualitiesPref, value);
    if (lossy_prefs_writing_task_posted_)
      return;

    // Post the task that schedules the writing of the lossy prefs.
    lossy_prefs_writing_task_posted_ = true;

    // Delay after which the task that schedules the writing of the lossy prefs.
    // This is needed in case the writing of the lossy prefs is not scheduled
    // automatically. The delay was chosen so that it is large enough that it
    // does not affect the startup performance.
    static const int32_t kUpdatePrefsDelaySeconds = 10;

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(
            &NetworkQualitiesPrefDelegateImpl::SchedulePendingLossyWrites,
            weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(kUpdatePrefsDelaySeconds));
  }
  std::unique_ptr<base::DictionaryValue> GetDictionaryValue() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    UMA_HISTOGRAM_EXACT_LINEAR("NQE.Prefs.ReadCount", 1, 2);
    return pref_service_->GetDictionary(kNetworkQualitiesPref)
        ->CreateDeepCopy();
  }

 private:
  // Schedules the writing of the lossy prefs.
  void SchedulePendingLossyWrites() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    UMA_HISTOGRAM_EXACT_LINEAR("NQE.Prefs.WriteCount", 1, 2);
    pref_service_->SchedulePendingLossyWrites();
    lossy_prefs_writing_task_posted_ = false;
  }

  PrefService* pref_service_;

  // True if the task that schedules the writing of the lossy prefs has been
  // posted.
  bool lossy_prefs_writing_task_posted_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<NetworkQualitiesPrefDelegateImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkQualitiesPrefDelegateImpl);
};

// Connects the SdchOwner's storage to the prefs.
class SdchOwnerPrefStorage : public net::SdchOwner::PrefStorage,
                             public PrefStore::Observer {
 public:
  explicit SdchOwnerPrefStorage(PersistentPrefStore* storage)
      : storage_(storage), storage_key_("SDCH"), init_observer_(nullptr) {}
  ~SdchOwnerPrefStorage() override {
    if (init_observer_)
      storage_->RemoveObserver(this);
  }

  ReadError GetReadError() const override {
    PersistentPrefStore::PrefReadError error = storage_->GetReadError();

    DCHECK_NE(
        error,
        PersistentPrefStore::PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE);
    DCHECK_NE(error, PersistentPrefStore::PREF_READ_ERROR_MAX_ENUM);

    switch (error) {
      case PersistentPrefStore::PREF_READ_ERROR_NONE:
        return PERSISTENCE_FAILURE_NONE;

      case PersistentPrefStore::PREF_READ_ERROR_NO_FILE:
        return PERSISTENCE_FAILURE_REASON_NO_FILE;

      case PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE:
      case PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE:
      case PersistentPrefStore::PREF_READ_ERROR_FILE_OTHER:
      case PersistentPrefStore::PREF_READ_ERROR_FILE_LOCKED:
      case PersistentPrefStore::PREF_READ_ERROR_JSON_REPEAT:
        return PERSISTENCE_FAILURE_REASON_READ_FAILED;

      case PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED:
      case PersistentPrefStore::PREF_READ_ERROR_FILE_NOT_SPECIFIED:
      case PersistentPrefStore::PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE:
      case PersistentPrefStore::PREF_READ_ERROR_MAX_ENUM:
      default:
        // We don't expect these other failures given our usage of prefs.
        NOTREACHED();
        return PERSISTENCE_FAILURE_REASON_OTHER;
    }
  }

  bool GetValue(const base::DictionaryValue** result) const override {
    const base::Value* result_value = nullptr;
    if (!storage_->GetValue(storage_key_, &result_value))
      return false;
    return result_value->GetAsDictionary(result);
  }

  bool GetMutableValue(base::DictionaryValue** result) override {
    base::Value* result_value = nullptr;
    if (!storage_->GetMutableValue(storage_key_, &result_value))
      return false;
    return result_value->GetAsDictionary(result);
  }

  void SetValue(std::unique_ptr<base::DictionaryValue> value) override {
    storage_->SetValue(storage_key_, std::move(value),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }

  void ReportValueChanged() override {
    storage_->ReportValueChanged(storage_key_,
                                 WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }

  bool IsInitializationComplete() override {
    return storage_->IsInitializationComplete();
  }

  void StartObservingInit(net::SdchOwner* observer) override {
    DCHECK(!init_observer_);
    init_observer_ = observer;
    storage_->AddObserver(this);
  }

  void StopObservingInit() override {
    DCHECK(init_observer_);
    init_observer_ = nullptr;
    storage_->RemoveObserver(this);
  }

 private:
  // PrefStore::Observer implementation.
  void OnPrefValueChanged(const std::string& key) override {}
  void OnInitializationCompleted(bool succeeded) override {
    init_observer_->OnPrefStorageInitializationComplete(succeeded);
  }

  PersistentPrefStore* storage_;  // Non-owning.
  const std::string storage_key_;

  net::SdchOwner* init_observer_;  // Non-owning.

  DISALLOW_COPY_AND_ASSIGN(SdchOwnerPrefStorage);
};

}  // namespace

CronetPrefsManager::CronetPrefsManager(
    const std::string& storage_path,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    bool enable_network_quality_estimator,
    bool enable_host_cache_persistence,
    net::NetLog* net_log,
    net::URLRequestContextBuilder* context_builder)
    : http_server_properties_manager_(nullptr) {
  DCHECK(network_task_runner->BelongsToCurrentThread());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::FilePath storage_file_path(storage_path);

  // Make sure storage directory has correct version.
  InitializeStorageDirectory(storage_file_path);
  base::FilePath filepath =
      storage_file_path.Append(FILE_PATH_LITERAL(kPrefsDirectoryName))
          .Append(FILE_PATH_LITERAL(kPrefsFileName));

  json_pref_store_ = new JsonPrefStore(filepath, file_task_runner,
                                       std::unique_ptr<PrefFilter>());

  // Register prefs and set up the PrefService.
  PrefServiceFactory factory;
  factory.set_user_prefs(json_pref_store_);
  scoped_refptr<PrefRegistrySimple> registry(new PrefRegistrySimple());
  registry->RegisterDictionaryPref(kHttpServerPropertiesPref,
                                   base::MakeUnique<base::DictionaryValue>());

  if (enable_network_quality_estimator) {
    // Use lossy prefs to limit the overhead of reading/writing the prefs.
    registry->RegisterDictionaryPref(kNetworkQualitiesPref,
                                     PrefRegistry::LOSSY_PREF);
  }

  if (enable_host_cache_persistence) {
    registry->RegisterListPref(kHostCachePref);
  }

  {
    SCOPED_UMA_HISTOGRAM_TIMER("Net.Cronet.PrefsInitTime");
    pref_service_ = factory.Create(registry.get());
  }

  http_server_properties_manager_ = new net::HttpServerPropertiesManager(
      new PrefServiceAdapter(pref_service_.get()), network_task_runner,
      network_task_runner, net_log);
  http_server_properties_manager_->InitializeOnNetworkSequence();

  // Passes |http_server_properties_manager_| ownership to |context_builder|.
  // The ownership will be subsequently passed to UrlRequestContext.
  context_builder->SetHttpServerProperties(
      std::unique_ptr<net::HttpServerPropertiesManager>(
          http_server_properties_manager_));
}

CronetPrefsManager::~CronetPrefsManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void CronetPrefsManager::SetupNqePersistence(
    net::NetworkQualityEstimator* nqe) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  network_qualities_prefs_manager_ =
      base::MakeUnique<net::NetworkQualitiesPrefsManager>(
          base::MakeUnique<NetworkQualitiesPrefDelegateImpl>(
              pref_service_.get()));

  network_qualities_prefs_manager_->InitializeOnNetworkThread(nqe);
}

void CronetPrefsManager::SetupHostCachePersistence(
    net::HostCache* host_cache,
    int host_cache_persistence_delay_ms,
    net::NetLog* net_log) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  host_cache_persistence_manager_ =
      base::MakeUnique<HostCachePersistenceManager>(
          host_cache, pref_service_.get(), kHostCachePref,
          base::TimeDelta::FromMilliseconds(host_cache_persistence_delay_ms),
          net_log);
}

void CronetPrefsManager::SetupSdchPersistence(net::SdchOwner* sdch_owner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  sdch_owner->EnablePersistentStorage(
      base::MakeUnique<SdchOwnerPrefStorage>(json_pref_store_.get()));
}

void CronetPrefsManager::PrepareForShutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (pref_service_)
    pref_service_->CommitPendingWrite();

  // Shutdown managers on the Pref sequence.
  http_server_properties_manager_->ShutdownOnPrefSequence();
  if (network_qualities_prefs_manager_)
    network_qualities_prefs_manager_->ShutdownOnPrefSequence();

  // TODO(crbug.com/758711): revisit to see whether the logic can be simplified
  // after SDCH is removed. Destroy |host_cache_persistence_manager_| before the
  // caller destroys UrlRequestContext.
  host_cache_persistence_manager_.reset(nullptr);
}

}  // namespace cronet
