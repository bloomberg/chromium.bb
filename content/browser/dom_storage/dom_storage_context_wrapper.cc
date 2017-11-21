// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"

#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/memory_coordinator_client_registry.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/local_storage_context_mojo.h"
#include "content/browser/dom_storage/session_storage_context_mojo.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {
namespace {

const char kLocalStorageDirectory[] = "Local Storage";
const char kSessionStorageDirectory[] = "Session Storage";

void InvokeLocalStorageUsageCallbackHelper(
    const DOMStorageContext::GetLocalStorageUsageCallback& callback,
    std::unique_ptr<std::vector<LocalStorageUsageInfo>> infos) {
  callback.Run(*infos);
}

void GetLocalStorageUsageHelper(
    base::SingleThreadTaskRunner* reply_task_runner,
    DOMStorageContextImpl* context,
    const DOMStorageContext::GetLocalStorageUsageCallback& callback) {
  auto infos = std::make_unique<std::vector<LocalStorageUsageInfo>>();
  context->GetLocalStorageUsage(infos.get(), true);
  reply_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&InvokeLocalStorageUsageCallbackHelper,
                                callback, std::move(infos)));
}

void CollectLocalStorageUsage(
    std::vector<LocalStorageUsageInfo>* out_info,
    base::Closure done_callback,
    const std::vector<LocalStorageUsageInfo>& in_info) {
  out_info->insert(out_info->end(), in_info.begin(), in_info.end());
  done_callback.Run();
}

void GotMojoLocalStorageUsage(
    scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner,
    const DOMStorageContext::GetLocalStorageUsageCallback& callback,
    std::vector<LocalStorageUsageInfo> usage) {
  reply_task_runner->PostTask(FROM_HERE,
                              base::BindOnce(callback, std::move(usage)));
}

void InvokeSessionStorageUsageCallbackHelper(
      const DOMStorageContext::GetSessionStorageUsageCallback& callback,
      const std::vector<SessionStorageUsageInfo>* infos) {
  callback.Run(*infos);
}

void GetSessionStorageUsageHelper(
    base::SingleThreadTaskRunner* reply_task_runner,
    DOMStorageContextImpl* context,
    const DOMStorageContext::GetSessionStorageUsageCallback& callback) {
  std::vector<SessionStorageUsageInfo>* infos =
      new std::vector<SessionStorageUsageInfo>;
  context->GetSessionStorageUsage(infos);
  reply_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&InvokeSessionStorageUsageCallbackHelper,
                                callback, base::Owned(infos)));
}

}  // namespace

DOMStorageContextWrapper::DOMStorageContextWrapper(
    service_manager::Connector* connector,
    const base::FilePath& profile_path,
    const base::FilePath& local_partition_path,
    storage::SpecialStoragePolicy* special_storage_policy) {
  base::FilePath data_path;
  if (!profile_path.empty())
    data_path = profile_path.Append(local_partition_path);

  scoped_refptr<base::SequencedTaskRunner> primary_sequence =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  scoped_refptr<base::SequencedTaskRunner> commit_sequence =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  context_ = new DOMStorageContextImpl(
      data_path.empty() ? data_path
                        : data_path.AppendASCII(kLocalStorageDirectory),
      data_path.empty() ? data_path
                        : data_path.AppendASCII(kSessionStorageDirectory),
      special_storage_policy,
      new DOMStorageWorkerPoolTaskRunner(std::move(primary_sequence),
                                         std::move(commit_sequence)));

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMojoLocalStorage)) {
    base::FilePath storage_dir;
    if (!profile_path.empty())
      storage_dir = local_partition_path.AppendASCII(kLocalStorageDirectory);
    // TODO(mek): Use a SequencedTaskRunner once mojo no longer requires a
    // SingleThreadTaskRunner (http://crbug.com/678155).
    mojo_task_runner_ =
        BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
    mojo_state_ = new LocalStorageContextMojo(
        mojo_task_runner_, connector, context_->task_runner(),
        data_path.empty() ? data_path
                          : data_path.AppendASCII(kLocalStorageDirectory),
        storage_dir, special_storage_policy);
  }

  if (base::FeatureList::IsEnabled(features::kMojoSessionStorage)) {
    base::FilePath session_storage_dir;
    if (!profile_path.empty())
      session_storage_dir =
          local_partition_path.AppendASCII(kSessionStorageDirectory);
    mojo_session_state_ = std::make_unique<SessionStorageContextMojo>(
        connector, session_storage_dir);
  }

  if (base::FeatureList::IsEnabled(features::kMemoryCoordinator)) {
    base::MemoryCoordinatorClientRegistry::GetInstance()->Register(this);
  } else {
    memory_pressure_listener_.reset(new base::MemoryPressureListener(
        base::Bind(&DOMStorageContextWrapper::OnMemoryPressure, this)));
  }
}

DOMStorageContextWrapper::~DOMStorageContextWrapper() {
  DCHECK(!mojo_state_) << "Shutdown should be called before destruction";
}

void DOMStorageContextWrapper::GetLocalStorageUsage(
    const GetLocalStorageUsageCallback& callback) {
  DCHECK(context_.get());
  if (mojo_state_) {
    auto infos = std::make_unique<std::vector<LocalStorageUsageInfo>>();
    auto* infos_ptr = infos.get();
    base::RepeatingClosure got_local_storage_usage = base::BarrierClosure(
        2, base::BindOnce(&InvokeLocalStorageUsageCallbackHelper, callback,
                          std::move(infos)));

    // base::Unretained is safe here, because the mojo_state_ won't be deleted
    // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
    // as soon as that task is posted, mojo_state_ is set to null, preventing
    // further tasks from being queued.
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LocalStorageContextMojo::GetStorageUsage,
            base::Unretained(mojo_state_),
            base::BindOnce(&GotMojoLocalStorageUsage,
                           base::ThreadTaskRunnerHandle::Get(),
                           base::Bind(CollectLocalStorageUsage, infos_ptr,
                                      got_local_storage_usage))));
    context_->task_runner()->PostShutdownBlockingTask(
        FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
        base::BindOnce(&GetLocalStorageUsageHelper,
                       base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                       base::RetainedRef(context_),
                       base::Bind(&CollectLocalStorageUsage, infos_ptr,
                                  got_local_storage_usage)));
    return;
  }
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&GetLocalStorageUsageHelper,
                     base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                     base::RetainedRef(context_), callback));
}

void DOMStorageContextWrapper::GetSessionStorageUsage(
    const GetSessionStorageUsageCallback& callback) {
  DCHECK(context_.get());
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&GetSessionStorageUsageHelper,
                     base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                     base::RetainedRef(context_), callback));
}

void DOMStorageContextWrapper::DeleteLocalStorageForPhysicalOrigin(
    const GURL& origin) {
  DCHECK(context_.get());
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(
          &DOMStorageContextImpl::DeleteLocalStorageForPhysicalOrigin, context_,
          origin));
  if (mojo_state_) {
    // base::Unretained is safe here, because the mojo_state_ won't be deleted
    // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
    // as soon as that task is posted, mojo_state_ is set to null, preventing
    // further tasks from being queued.
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalStorageContextMojo::DeleteStorageForPhysicalOrigin,
                       base::Unretained(mojo_state_),
                       url::Origin::Create(origin)));
  }
}

void DOMStorageContextWrapper::DeleteLocalStorage(const GURL& origin) {
  DCHECK(context_.get());
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&DOMStorageContextImpl::DeleteLocalStorage, context_,
                     origin));
  if (mojo_state_) {
    // base::Unretained is safe here, because the mojo_state_ won't be deleted
    // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
    // as soon as that task is posted, mojo_state_ is set to null, preventing
    // further tasks from being queued.
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&LocalStorageContextMojo::DeleteStorage,
                                  base::Unretained(mojo_state_),
                                  url::Origin::Create(origin)));
  }
}

void DOMStorageContextWrapper::DeleteSessionStorage(
    const SessionStorageUsageInfo& usage_info) {
  DCHECK(context_.get());
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&DOMStorageContextImpl::DeleteSessionStorage, context_,
                     usage_info));
}

void DOMStorageContextWrapper::SetSaveSessionStorageOnDisk() {
  DCHECK(context_.get());
  context_->SetSaveSessionStorageOnDisk();
}

scoped_refptr<SessionStorageNamespace>
DOMStorageContextWrapper::RecreateSessionStorage(
    const std::string& persistent_id) {
  return scoped_refptr<SessionStorageNamespace>(
      new SessionStorageNamespaceImpl(this, persistent_id));
}

void DOMStorageContextWrapper::StartScavengingUnusedSessionStorage() {
  DCHECK(context_.get());
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(
          &DOMStorageContextImpl::StartScavengingUnusedSessionStorage,
          context_));
}

void DOMStorageContextWrapper::SetForceKeepSessionState() {
  DCHECK(context_.get());
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&DOMStorageContextImpl::SetForceKeepSessionState,
                     context_));
  if (mojo_state_) {
    // base::Unretained is safe here, because the mojo_state_ won't be deleted
    // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
    // as soon as that task is posted, mojo_state_ is set to null, preventing
    // further tasks from being queued.
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalStorageContextMojo::SetForceKeepSessionState,
                       base::Unretained(mojo_state_)));
  }
}

void DOMStorageContextWrapper::Shutdown() {
  DCHECK(context_.get());
  if (mojo_state_) {
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&LocalStorageContextMojo::ShutdownAndDelete,
                                  base::Unretained(mojo_state_)));
    mojo_state_ = nullptr;
  }
  memory_pressure_listener_.reset();
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&DOMStorageContextImpl::Shutdown, context_));
  if (base::FeatureList::IsEnabled(features::kMemoryCoordinator)) {
    base::MemoryCoordinatorClientRegistry::GetInstance()->Unregister(this);
  }
}

void DOMStorageContextWrapper::Flush() {
  DCHECK(context_.get());

  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&DOMStorageContextImpl::Flush, context_));
  if (mojo_state_) {
    // base::Unretained is safe here, because the mojo_state_ won't be deleted
    // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
    // as soon as that task is posted, mojo_state_ is set to null, preventing
    // further tasks from being queued.
    mojo_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&LocalStorageContextMojo::Flush,
                                               base::Unretained(mojo_state_)));
  }
}

void DOMStorageContextWrapper::OpenLocalStorage(
    const url::Origin& origin,
    mojom::LevelDBWrapperRequest request) {
  if (!mojo_state_)
    return;
  // base::Unretained is safe here, because the mojo_state_ won't be deleted
  // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
  // as soon as that task is posted, mojo_state_ is set to null, preventing
  // further tasks from being queued.
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LocalStorageContextMojo::OpenLocalStorage,
                                base::Unretained(mojo_state_), origin,
                                std::move(request)));
}

void DOMStorageContextWrapper::OpenSessionStorage(
    int64_t namespace_id,
    const url::Origin& origin,
    mojom::LevelDBWrapperRequest request) {
  if (!mojo_session_state_)
    return;
  mojo_session_state_->OpenSessionStorage(namespace_id, origin,
                                          std::move(request));
}

void DOMStorageContextWrapper::SetLocalStorageDatabaseForTesting(
    leveldb::mojom::LevelDBDatabaseAssociatedPtr database) {
  if (!mojo_state_)
    return;
  // base::Unretained is safe here, because the mojo_state_ won't be deleted
  // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
  // as soon as that task is posted, mojo_state_ is set to null, preventing
  // further tasks from being queued.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalStorageContextMojo::SetDatabaseForTesting,
                     base::Unretained(mojo_state_), std::move(database)));
}

base::WeakPtr<SessionStorageContextMojo>
DOMStorageContextWrapper::GetMojoSessionStateWeakPtr() {
  return mojo_session_state_ ? mojo_session_state_->AsWeakPtr()
                             : base::WeakPtr<SessionStorageContextMojo>();
}

void DOMStorageContextWrapper::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  DOMStorageContextImpl::PurgeOption purge_option =
      DOMStorageContextImpl::PURGE_UNOPENED;
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    purge_option = DOMStorageContextImpl::PURGE_AGGRESSIVE;
  }
  PurgeMemory(purge_option);
}

void DOMStorageContextWrapper::OnPurgeMemory() {
  PurgeMemory(DOMStorageContextImpl::PURGE_AGGRESSIVE);
}

void DOMStorageContextWrapper::PurgeMemory(DOMStorageContextImpl::PurgeOption
    purge_option) {
  context_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DOMStorageContextImpl::PurgeMemory, context_,
                                purge_option));
  if (mojo_state_ && purge_option == DOMStorageContextImpl::PURGE_AGGRESSIVE) {
    // base::Unretained is safe here, because the mojo_state_ won't be deleted
    // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
    // as soon as that task is posted, mojo_state_ is set to null, preventing
    // further tasks from being queued.
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&LocalStorageContextMojo::PurgeMemory,
                                  base::Unretained(mojo_state_)));
  }
}

}  // namespace content
