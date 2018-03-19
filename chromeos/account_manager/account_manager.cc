// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/account_manager/account_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "chromeos/account_manager/tokens.pb.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace chromeos {

namespace {

constexpr base::FilePath::CharType kTokensFileName[] =
    FILE_PATH_LITERAL("AccountManagerTokens.bin");
constexpr int kTokensFileMaxSizeInBytes = 100000;  // ~100 KB

AccountManager::TokenMap LoadTokensFromDisk(
    const base::FilePath& tokens_file_path) {
  AccountManager::TokenMap tokens;

  VLOG(1) << "AccountManager::LoadTokensFromDisk";
  std::string token_file_data;
  bool success = ReadFileToStringWithMaxSize(tokens_file_path, &token_file_data,
                                             kTokensFileMaxSizeInBytes);
  if (!success) {
    LOG(ERROR) << "Failed to read tokens file";
    return tokens;
  }

  chromeos::account_manager::Tokens tokens_proto;
  success = tokens_proto.ParseFromString(token_file_data);
  if (!success) {
    LOG(ERROR) << "Failed to parse tokens from file";
    return tokens;
  }

  tokens.insert(tokens_proto.login_scoped_tokens().begin(),
                tokens_proto.login_scoped_tokens().end());

  return tokens;
}

std::string GetSerializedTokens(const AccountManager::TokenMap& tokens) {
  chromeos::account_manager::Tokens tokens_proto;
  *tokens_proto.mutable_login_scoped_tokens() =
      ::google::protobuf::Map<std::string, std::string>(tokens.begin(),
                                                        tokens.end());
  return tokens_proto.SerializeAsString();
}

}  // namespace

AccountManager::AccountManager() : weak_factory_(this) {}

void AccountManager::Initialize(const base::FilePath& home_dir) {
  Initialize(home_dir, base::CreateSequencedTaskRunnerWithTraits(
                           {base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
                            base::MayBlock()}));
}

void AccountManager::Initialize(
    const base::FilePath& home_dir,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  VLOG(1) << "AccountManager::Initialize";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (init_state_ != InitializationState::kNotStarted) {
    // |Initialize| has already been called once. To help diagnose possible race
    // conditions, check whether the |home_dir| parameter provided by the first
    // invocation of |Initialize| matches the one it is currently being called
    // with.
    DCHECK_EQ(home_dir, writer_->path().DirName());
    return;
  }

  init_state_ = InitializationState::kInProgress;
  task_runner_ = task_runner;
  writer_ = std::make_unique<base::ImportantFileWriter>(
      home_dir.Append(kTokensFileName), task_runner_);

  PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&LoadTokensFromDisk, writer_->path()),
      base::BindOnce(&AccountManager::InsertTokensAndRunInitializationCallbacks,
                     weak_factory_.GetWeakPtr()));
}

void AccountManager::InsertTokensAndRunInitializationCallbacks(
    const AccountManager::TokenMap& tokens) {
  VLOG(1) << "AccountManager::InsertTokensAndRunInitializationCallbacks";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tokens_.insert(tokens.begin(), tokens.end());
  init_state_ = InitializationState::kInitialized;

  for (auto& cb : initialization_callbacks_) {
    std::move(cb).Run();
  }
  initialization_callbacks_.clear();
}

AccountManager::~AccountManager() {
  // AccountManager is supposed to be used as a leaky global.
}

void AccountManager::RunOnInitialization(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (init_state_ != InitializationState::kInitialized) {
    initialization_callbacks_.emplace_back(std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

void AccountManager::GetAccounts(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  base::OnceClosure closure =
      base::BindOnce(&AccountManager::GetAccountsInternal,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  RunOnInitialization(std::move(closure));
}

void AccountManager::GetAccountsInternal(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  std::vector<std::string> accounts;
  accounts.reserve(tokens_.size());
  for (auto& key_val : tokens_) {
    accounts.emplace_back(key_val.first);
  }
  std::move(callback).Run(std::move(accounts));
}

void AccountManager::UpsertToken(const std::string& account_id,
                                 const std::string& login_scoped_token) {
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  base::OnceClosure closure = base::BindOnce(
      &AccountManager::UpsertTokenInternal, weak_factory_.GetWeakPtr(),
      account_id, login_scoped_token);
  RunOnInitialization(std::move(closure));
}

void AccountManager::UpsertTokenInternal(
    const std::string& account_id,
    const std::string& login_scoped_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  tokens_[account_id] = login_scoped_token;
  PersistTokensAsync();
}

void AccountManager::PersistTokensAsync() {
  // Schedule (immediately) a non-blocking write.
  writer_->WriteNow(
      std::make_unique<std::string>(GetSerializedTokens(tokens_)));
}

}  // namespace chromeos
