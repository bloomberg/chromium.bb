// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sessions/core/command_storage_backend.h"
#include "components/sessions/core/command_storage_manager_delegate.h"
#include "crypto/random.h"

namespace sessions {
namespace {

// Helper used by ScheduleGetLastSessionCommands. It runs callback on TaskRunner
// thread if it's not canceled.
void RunIfNotCanceled(
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
    CommandStorageManager::GetCommandsCallback callback,
    std::vector<std::unique_ptr<SessionCommand>> commands) {
  if (is_canceled.Run())
    return;
  std::move(callback).Run(std::move(commands));
}

void PostOrRunInternalGetCommandsCallback(
    base::SequencedTaskRunner* task_runner,
    CommandStorageManager::GetCommandsCallback callback,
    std::vector<std::unique_ptr<SessionCommand>> commands) {
  if (task_runner->RunsTasksInCurrentSequence()) {
    std::move(callback).Run(std::move(commands));
  } else {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(commands)));
  }
}

}  // namespace

// Delay between when a command is received, and when we save it to the
// backend.
constexpr base::TimeDelta kSaveDelay = base::TimeDelta::FromMilliseconds(2500);

CommandStorageManager::CommandStorageManager(
    const base::FilePath& path,
    CommandStorageManagerDelegate* delegate,
    bool enable_crypto)
    : CommandStorageManager(base::MakeRefCounted<CommandStorageBackend>(
                                CreateDefaultBackendTaskRunner(),
                                path),
                            delegate) {
  use_crypto_ = enable_crypto;
}

CommandStorageManager::~CommandStorageManager() = default;

// static
std::vector<uint8_t> CommandStorageManager::CreateCryptoKey() {
  std::vector<uint8_t> key(32);
  crypto::RandBytes(&(key.front()), key.size());
  return key;
}

void CommandStorageManager::ScheduleCommand(
    std::unique_ptr<SessionCommand> command) {
  DCHECK(command);
  commands_since_reset_++;
  pending_commands_.push_back(std::move(command));
  StartSaveTimer();
}

void CommandStorageManager::AppendRebuildCommand(
    std::unique_ptr<SessionCommand> command) {
  std::vector<std::unique_ptr<SessionCommand>> commands;
  commands.push_back(std::move(command));
  AppendRebuildCommands(std::move(commands));
}

void CommandStorageManager::AppendRebuildCommands(
    std::vector<std::unique_ptr<SessionCommand>> commands) {
  pending_commands_.insert(pending_commands_.end(),
                           std::make_move_iterator(commands.begin()),
                           std::make_move_iterator(commands.end()));
}

void CommandStorageManager::EraseCommand(SessionCommand* old_command) {
  auto it = std::find_if(
      pending_commands_.begin(), pending_commands_.end(),
      [old_command](const std::unique_ptr<SessionCommand>& command_ptr) {
        return command_ptr.get() == old_command;
      });
  CHECK(it != pending_commands_.end());
  pending_commands_.erase(it);
}

void CommandStorageManager::SwapCommand(
    SessionCommand* old_command,
    std::unique_ptr<SessionCommand> new_command) {
  auto it = std::find_if(
      pending_commands_.begin(), pending_commands_.end(),
      [old_command](const std::unique_ptr<SessionCommand>& command_ptr) {
        return command_ptr.get() == old_command;
      });
  CHECK(it != pending_commands_.end());
  *it = std::move(new_command);
}

void CommandStorageManager::ClearPendingCommands() {
  pending_commands_.clear();
}

void CommandStorageManager::StartSaveTimer() {
  // Don't start a timer when testing.
  if (delegate_->ShouldUseDelayedSave() &&
      base::ThreadTaskRunnerHandle::IsSet() && !HasPendingSave()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CommandStorageManager::Save,
                       weak_factory_for_timer_.GetWeakPtr()),
        kSaveDelay);
  }
}

void CommandStorageManager::Save() {
  // Inform the delegate that we will save the commands now, giving it the
  // opportunity to append more commands.
  delegate_->OnWillSaveCommands();

  if (pending_commands_.empty())
    return;

  std::vector<uint8_t> crypto_key;
  if (use_crypto_ && pending_reset_) {
    crypto_key = CreateCryptoKey();
    delegate_->OnGeneratedNewCryptoKey(crypto_key);
  }
  backend_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&CommandStorageBackend::AppendCommands, backend_,
                     std::move(pending_commands_), pending_reset_, crypto_key));

  if (pending_reset_) {
    commands_since_reset_ = 0;
    pending_reset_ = false;
  }
}

bool CommandStorageManager::HasPendingSave() const {
  return weak_factory_for_timer_.HasWeakPtrs();
}

base::CancelableTaskTracker::TaskId
CommandStorageManager::ScheduleGetCurrentSessionCommands(
    GetCommandsCallback callback,
    const std::vector<uint8_t>& decryption_key,
    base::CancelableTaskTracker* tracker) {
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  GetCommandsCallback backend_callback;
  const base::CancelableTaskTracker::TaskId id = CreateCallbackForGetCommands(
      tracker, std::move(callback), &is_canceled, &backend_callback);

  backend_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&CommandStorageBackend::ReadCurrentSessionCommands,
                     backend_.get(), is_canceled, decryption_key,
                     std::move(backend_callback)));
  return id;
}

CommandStorageManager::CommandStorageManager(
    scoped_refptr<CommandStorageBackend> backend,
    CommandStorageManagerDelegate* delegate)
    : backend_(std::move(backend)),
      delegate_(delegate),
      backend_task_runner_(backend_->owning_task_runner()) {}

// static
scoped_refptr<base::SequencedTaskRunner>
CommandStorageManager::CreateDefaultBackendTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}

base::CancelableTaskTracker::TaskId
CommandStorageManager::CreateCallbackForGetCommands(
    base::CancelableTaskTracker* tracker,
    GetCommandsCallback callback,
    base::CancelableTaskTracker::IsCanceledCallback* is_canceled,
    GetCommandsCallback* backend_callback) {
  const base::CancelableTaskTracker::TaskId id =
      tracker->NewTrackedTaskId(is_canceled);

  GetCommandsCallback run_if_not_canceled =
      base::BindOnce(&RunIfNotCanceled, *is_canceled, std::move(callback));

  *backend_callback =
      base::BindOnce(&PostOrRunInternalGetCommandsCallback,
                     base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                     std::move(run_if_not_canceled));
  return id;
}

}  // namespace sessions
