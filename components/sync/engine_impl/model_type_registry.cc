// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/model_type_registry.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/base/cryptographer.h"
#include "components/sync/engine/activation_context.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/engine_impl/cycle/directory_type_debug_info_emitter.h"
#include "components/sync/engine_impl/cycle/non_blocking_type_debug_info_emitter.h"
#include "components/sync/engine_impl/directory_commit_contributor.h"
#include "components/sync/engine_impl/directory_update_handler.h"
#include "components/sync/engine_impl/model_type_worker.h"

namespace syncer {

namespace {

class CommitQueueProxy : public CommitQueue {
 public:
  CommitQueueProxy(const base::WeakPtr<ModelTypeWorker>& worker,
                   const scoped_refptr<base::SequencedTaskRunner>& sync_thread);
  ~CommitQueueProxy() override;

  void EnqueueForCommit(const CommitRequestDataList& list) override;

 private:
  base::WeakPtr<ModelTypeWorker> worker_;
  scoped_refptr<base::SequencedTaskRunner> sync_thread_;
};

CommitQueueProxy::CommitQueueProxy(
    const base::WeakPtr<ModelTypeWorker>& worker,
    const scoped_refptr<base::SequencedTaskRunner>& sync_thread)
    : worker_(worker), sync_thread_(sync_thread) {}

CommitQueueProxy::~CommitQueueProxy() {}

void CommitQueueProxy::EnqueueForCommit(const CommitRequestDataList& list) {
  sync_thread_->PostTask(
      FROM_HERE, base::Bind(&ModelTypeWorker::EnqueueForCommit, worker_, list));
}

}  // namespace

ModelTypeRegistry::ModelTypeRegistry(
    const std::vector<scoped_refptr<ModelSafeWorker>>& workers,
    UserShare* user_share,
    NudgeHandler* nudge_handler,
    const UssMigrator& uss_migrator)
    : user_share_(user_share),
      nudge_handler_(nudge_handler),
      uss_migrator_(uss_migrator),
      weak_ptr_factory_(this) {
  for (size_t i = 0u; i < workers.size(); ++i) {
    workers_map_.insert(
        std::make_pair(workers[i]->GetModelSafeGroup(), workers[i]));
  }
}

ModelTypeRegistry::~ModelTypeRegistry() {}

void ModelTypeRegistry::SetEnabledDirectoryTypes(
    const ModelSafeRoutingInfo& routing_info) {
  // Remove all existing directory processors and delete them.  The
  // DebugInfoEmitters are not deleted here, since we want to preserve their
  // counters.
  for (ModelTypeSet::Iterator it = enabled_directory_types_.First(); it.Good();
       it.Inc()) {
    size_t result1 = update_handler_map_.erase(it.Get());
    size_t result2 = commit_contributor_map_.erase(it.Get());
    DCHECK_EQ(1U, result1);
    DCHECK_EQ(1U, result2);
  }

  // Clear the old instances of directory update handlers and commit
  // contributors, deleting their contents in the processs.
  directory_update_handlers_.clear();
  directory_commit_contributors_.clear();

  enabled_directory_types_.Clear();

  // Create new ones and add them to the appropriate containers.
  for (const auto& routing_kv : routing_info) {
    ModelType type = routing_kv.first;
    ModelSafeGroup group = routing_kv.second;
    if (group == GROUP_NON_BLOCKING)
      continue;

    std::map<ModelSafeGroup, scoped_refptr<ModelSafeWorker>>::iterator
        worker_it = workers_map_.find(group);
    DCHECK(worker_it != workers_map_.end());
    scoped_refptr<ModelSafeWorker> worker = worker_it->second;

    DataTypeDebugInfoEmitter* emitter = GetEmitter(type);
    if (emitter == nullptr) {
      auto new_emitter = base::MakeUnique<DirectoryTypeDebugInfoEmitter>(
          directory(), type, &type_debug_info_observers_);
      emitter = new_emitter.get();
      data_type_debug_info_emitter_map_.insert(
          std::make_pair(type, std::move(new_emitter)));
    }

    auto updater = base::MakeUnique<DirectoryUpdateHandler>(directory(), type,
                                                            worker, emitter);
    bool updater_inserted =
        update_handler_map_.insert(std::make_pair(type, updater.get())).second;
    DCHECK(updater_inserted)
        << "Attempt to override existing type handler in map";
    directory_update_handlers_.push_back(std::move(updater));

    auto committer = base::MakeUnique<DirectoryCommitContributor>(
        directory(), type, emitter);
    bool committer_inserted =
        commit_contributor_map_.insert(std::make_pair(type, committer.get()))
            .second;
    DCHECK(committer_inserted)
        << "Attempt to override existing type handler in map";
    directory_commit_contributors_.push_back(std::move(committer));

    enabled_directory_types_.Put(type);
  }

  DCHECK(Intersection(GetEnabledDirectoryTypes(), GetEnabledNonBlockingTypes())
             .Empty());
}

void ModelTypeRegistry::ConnectType(
    ModelType type,
    std::unique_ptr<ActivationContext> activation_context) {
  DCHECK(update_handler_map_.find(type) == update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) == commit_contributor_map_.end());
  DVLOG(1) << "Enabling an off-thread sync type: " << ModelTypeToString(type);

  bool initial_sync_done =
      activation_context->model_type_state.initial_sync_done();
  // Attempt migration if the USS initial sync hasn't been done, there is a
  // migrator function, and directory has data for this type.
  // Note: The injected migrator function is currently null outside of testing
  // until issues with triggering initial sync correctly are addressed.
  bool do_migration = !initial_sync_done && !uss_migrator_.is_null() &&
                      directory()->InitialSyncEndedForType(type);
  bool trigger_initial_sync = !initial_sync_done && !do_migration;

  // Save a raw pointer to the processor for connecting later.
  ModelTypeProcessor* type_processor = activation_context->type_processor.get();

  std::unique_ptr<Cryptographer> cryptographer_copy;
  if (encrypted_types_.Has(type))
    cryptographer_copy = base::MakeUnique<Cryptographer>(*cryptographer_);

  DataTypeDebugInfoEmitter* emitter = GetEmitter(type);
  if (emitter == nullptr) {
    auto new_emitter = base::MakeUnique<NonBlockingTypeDebugInfoEmitter>(
        type, &type_debug_info_observers_);
    emitter = new_emitter.get();
    data_type_debug_info_emitter_map_.insert(
        std::make_pair(type, std::move(new_emitter)));
  }

  auto worker = base::MakeUnique<ModelTypeWorker>(
      type, activation_context->model_type_state, trigger_initial_sync,
      std::move(cryptographer_copy), nudge_handler_,
      std::move(activation_context->type_processor), emitter);

  // Save a raw pointer and add the worker to our structures.
  ModelTypeWorker* worker_ptr = worker.get();
  model_type_workers_.push_back(std::move(worker));
  update_handler_map_.insert(std::make_pair(type, worker_ptr));
  commit_contributor_map_.insert(std::make_pair(type, worker_ptr));

  // Initialize Processor -> Worker communication channel.
  type_processor->ConnectSync(base::MakeUnique<CommitQueueProxy>(
      worker_ptr->AsWeakPtr(), base::ThreadTaskRunnerHandle::Get()));

  // Attempt migration if necessary.
  if (do_migration) {
    // TODO(crbug.com/658002): Store a pref before attempting migration
    // indicating that it was attempted so we can avoid failure loops.
    if (uss_migrator_.Run(type, user_share_, worker_ptr)) {
      UMA_HISTOGRAM_ENUMERATION("Sync.USSMigrationSuccess", type,
                                MODEL_TYPE_COUNT);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Sync.USSMigrationFailure", type,
                                MODEL_TYPE_COUNT);
    }
  }

  // TODO(crbug.com/658002): Delete directory data here if initial_sync_done and
  // has_directory_data are both true.

  DCHECK(Intersection(GetEnabledDirectoryTypes(), GetEnabledNonBlockingTypes())
             .Empty());
}

void ModelTypeRegistry::DisconnectType(ModelType type) {
  DVLOG(1) << "Disabling an off-thread sync type: " << ModelTypeToString(type);
  DCHECK(update_handler_map_.find(type) != update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) != commit_contributor_map_.end());

  size_t updaters_erased = update_handler_map_.erase(type);
  size_t committers_erased = commit_contributor_map_.erase(type);

  DCHECK_EQ(1U, updaters_erased);
  DCHECK_EQ(1U, committers_erased);

  auto iter = model_type_workers_.begin();
  while (iter != model_type_workers_.end()) {
    if ((*iter)->GetModelType() == type) {
      iter = model_type_workers_.erase(iter);
    } else {
      ++iter;
    }
  }
}

ModelTypeSet ModelTypeRegistry::GetEnabledTypes() const {
  return Union(GetEnabledDirectoryTypes(), GetEnabledNonBlockingTypes());
}

ModelTypeSet ModelTypeRegistry::GetInitialSyncEndedTypes() const {
  // TODO(pavely): GetInitialSyncEndedTypes is queried at the end of sync
  // manager initialization when update handlers aren't set up yet. Returning
  // correct set of types is important because otherwise data for al types will
  // be redownloaded during configuration. For now let's return union of types
  // reported by directory and types reported by update handlers. We need to
  // refactor initialization and configuratrion flow to be able to only query
  // this set from update handlers.
  ModelTypeSet result = directory()->InitialSyncEndedTypes();
  for (const auto& kv : update_handler_map_) {
    if (kv.second->IsInitialSyncEnded())
      result.Put(kv.first);
  }
  return result;
}

ModelTypeSet ModelTypeRegistry::GetInitialSyncDoneNonBlockingTypes() const {
  ModelTypeSet types;
  for (const auto& worker : model_type_workers_) {
    if (worker->IsInitialSyncEnded()) {
      types.Put(worker->GetModelType());
    }
  }
  return types;
}

UpdateHandlerMap* ModelTypeRegistry::update_handler_map() {
  return &update_handler_map_;
}

CommitContributorMap* ModelTypeRegistry::commit_contributor_map() {
  return &commit_contributor_map_;
}

void ModelTypeRegistry::RegisterDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {
  if (!type_debug_info_observers_.HasObserver(observer))
    type_debug_info_observers_.AddObserver(observer);
}

void ModelTypeRegistry::UnregisterDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {
  type_debug_info_observers_.RemoveObserver(observer);
}

bool ModelTypeRegistry::HasDirectoryTypeDebugInfoObserver(
    const TypeDebugInfoObserver* observer) const {
  return type_debug_info_observers_.HasObserver(observer);
}

void ModelTypeRegistry::RequestEmitDebugInfo() {
  for (const auto& kv : data_type_debug_info_emitter_map_) {
    kv.second->EmitCommitCountersUpdate();
    kv.second->EmitUpdateCountersUpdate();
    kv.second->EmitStatusCountersUpdate();
  }
}

base::WeakPtr<ModelTypeConnector> ModelTypeRegistry::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ModelTypeRegistry::OnPassphraseRequired(
    PassphraseRequiredReason reason,
    const sync_pb::EncryptedData& pending_keys) {}

void ModelTypeRegistry::OnPassphraseAccepted() {
  for (const auto& worker : model_type_workers_) {
    if (encrypted_types_.Has(worker->GetModelType())) {
      worker->EncryptionAcceptedApplyUpdates();
    }
  }
}

void ModelTypeRegistry::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token,
    BootstrapTokenType type) {}

void ModelTypeRegistry::OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                                                bool encrypt_everything) {
  // TODO(skym): This does not handle reducing the number of encrypted types
  // correctly. They're removed from |encrypted_types_| but corresponding
  // workers never have their Cryptographers removed. This probably is not a use
  // case that currently needs to be supported, but it should be guarded against
  // here.
  encrypted_types_ = encrypted_types;
  OnEncryptionStateChanged();
}

void ModelTypeRegistry::OnEncryptionComplete() {}

void ModelTypeRegistry::OnCryptographerStateChanged(
    Cryptographer* cryptographer) {
  cryptographer_ = base::MakeUnique<Cryptographer>(*cryptographer);
  OnEncryptionStateChanged();
}

void ModelTypeRegistry::OnPassphraseTypeChanged(PassphraseType type,
                                                base::Time passphrase_time) {}

void ModelTypeRegistry::OnLocalSetPassphraseEncryption(
    const SyncEncryptionHandler::NigoriState& nigori_state) {}

void ModelTypeRegistry::OnEncryptionStateChanged() {
  for (const auto& worker : model_type_workers_) {
    if (encrypted_types_.Has(worker->GetModelType())) {
      worker->UpdateCryptographer(
          base::MakeUnique<Cryptographer>(*cryptographer_));
    }
  }
}

DataTypeDebugInfoEmitter* ModelTypeRegistry::GetEmitter(ModelType type) {
  DataTypeDebugInfoEmitter* raw_emitter = nullptr;
  auto it = data_type_debug_info_emitter_map_.find(type);
  if (it != data_type_debug_info_emitter_map_.end()) {
    raw_emitter = it->second.get();
  }
  return raw_emitter;
}

ModelTypeSet ModelTypeRegistry::GetEnabledDirectoryTypes() const {
  return enabled_directory_types_;
}

ModelTypeSet ModelTypeRegistry::GetEnabledNonBlockingTypes() const {
  ModelTypeSet enabled_non_blocking_types;
  for (const auto& worker : model_type_workers_) {
    enabled_non_blocking_types.Put(worker->GetModelType());
  }
  return enabled_non_blocking_types;
}

}  // namespace syncer
