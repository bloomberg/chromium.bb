// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/account_manager/account_manager.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace chromeos {

namespace {

constexpr base::FilePath::CharType kTokensFileName[] =
    FILE_PATH_LITERAL("AccountManagerTokens.bin");
constexpr int kTokensFileMaxSizeInBytes = 100000;  // ~100 KB.

constexpr char kNumAccountsMetricName[] = "AccountManager.NumAccounts";
constexpr int kMaxNumAccountsMetric = 10;

AccountManager::TokenMap LoadTokensFromDisk(
    const base::FilePath& tokens_file_path) {
  AccountManager::TokenMap tokens;

  VLOG(1) << "AccountManager::LoadTokensFromDisk";
  std::string token_file_data;
  bool success = ReadFileToStringWithMaxSize(tokens_file_path, &token_file_data,
                                             kTokensFileMaxSizeInBytes);
  if (!success) {
    // TODO(sinhak): Add an error log when AccountManager becomes the default
    // Identity provider on Chrome OS.
    return tokens;
  }

  chromeos::account_manager::Accounts accounts_proto;
  success = accounts_proto.ParseFromString(token_file_data);
  if (!success) {
    LOG(ERROR) << "Failed to parse tokens from file";
    return tokens;
  }

  for (const auto& account : accounts_proto.accounts()) {
    AccountManager::AccountKey account_key{account.id(),
                                           account.account_type()};

    if (!account_key.IsValid()) {
      LOG(WARNING) << "Ignoring invalid account_key load from disk: "
                   << account_key;
      continue;
    }
    tokens[account_key] = account.token();
  }

  return tokens;
}

std::string GetSerializedTokens(const AccountManager::TokenMap& tokens) {
  chromeos::account_manager::Accounts accounts_proto;

  for (const auto& token : tokens) {
    account_manager::Account* account_proto = accounts_proto.add_accounts();
    account_proto->set_id(token.first.id);
    account_proto->set_account_type(token.first.account_type);
    account_proto->set_token(token.second);
  }

  return accounts_proto.SerializeAsString();
}

std::vector<AccountManager::AccountKey> GetAccountKeys(
    const AccountManager::TokenMap& tokens) {
  std::vector<AccountManager::AccountKey> accounts;
  accounts.reserve(tokens.size());

  for (const auto& key_val : tokens) {
    accounts.emplace_back(key_val.first);
  }

  return accounts;
}

void RecordNumAccountsMetric(const int num_accounts) {
  base::UmaHistogramExactLinear(kNumAccountsMetricName, num_accounts,
                                kMaxNumAccountsMetric + 1);
}

}  // namespace

// static
const char AccountManager::kActiveDirectoryDummyToken[] = "dummy_ad_token";

// static
const char* const AccountManager::kInvalidToken =
    OAuth2TokenServiceDelegate::kInvalidRefreshToken;

class AccountManager::GaiaTokenRevocationRequest : public GaiaAuthConsumer {
 public:
  GaiaTokenRevocationRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AccountManager::DelayNetworkCallRunner delay_network_call_runner,
      const std::string& refresh_token,
      base::WeakPtr<AccountManager> account_manager)
      : account_manager_(account_manager),
        refresh_token_(refresh_token),
        weak_factory_(this) {
    DCHECK(!refresh_token_.empty());
    gaia_auth_fetcher_ = std::make_unique<GaiaAuthFetcher>(
        this, gaia::GaiaSource::kChromeOS, url_loader_factory);
    base::RepeatingClosure start_revoke_token = base::BindRepeating(
        &GaiaTokenRevocationRequest::Start, weak_factory_.GetWeakPtr());
    delay_network_call_runner.Run(start_revoke_token);
  }

  ~GaiaTokenRevocationRequest() override = default;

  // GaiaAuthConsumer overrides.
  void OnOAuth2RevokeTokenCompleted(TokenRevocationStatus status) override {
    VLOG(1) << "GaiaTokenRevocationRequest::OnOAuth2RevokeTokenCompleted";
    // We cannot call |AccountManager::DeletePendingTokenRevocationRequest|
    // directly because it will immediately start deleting |this|, before the
    // method has had a chance to return.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AccountManager::DeletePendingTokenRevocationRequest,
                       account_manager_, this));
  }

 private:
  // Starts the actual work of sending a network request to revoke a token.
  void Start() { gaia_auth_fetcher_->StartRevokeOAuth2Token(refresh_token_); }

  // A weak pointer to |AccountManager|. The only purpose is to signal
  // the completion of work through
  // |AccountManager::DeletePendingTokenRevocationRequest|.
  base::WeakPtr<AccountManager> account_manager_;

  // Does the actual work of revoking a token.
  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;

  // Refresh token to be revoked from GAIA.
  std::string refresh_token_;

  base::WeakPtrFactory<GaiaTokenRevocationRequest> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(GaiaTokenRevocationRequest);
};

bool AccountManager::AccountKey::IsValid() const {
  return !id.empty() &&
         account_type != account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED;
}

bool AccountManager::AccountKey::operator<(const AccountKey& other) const {
  if (id != other.id) {
    return id < other.id;
  }

  return account_type < other.account_type;
}

bool AccountManager::AccountKey::operator==(const AccountKey& other) const {
  return id == other.id && account_type == other.account_type;
}

bool AccountManager::AccountKey::operator!=(const AccountKey& other) const {
  return !(*this == other);
}

AccountManager::Observer::Observer() = default;

AccountManager::Observer::~Observer() = default;

AccountManager::AccountManager() : weak_factory_(this) {}

void AccountManager::Initialize(
    const base::FilePath& home_dir,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DelayNetworkCallRunner delay_network_call_runner) {
  Initialize(
      home_dir, url_loader_factory, std::move(delay_network_call_runner),
      base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()}));
}

void AccountManager::Initialize(
    const base::FilePath& home_dir,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DelayNetworkCallRunner delay_network_call_runner,
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
  url_loader_factory_ = url_loader_factory;
  delay_network_call_runner_ = std::move(delay_network_call_runner);
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

  for (const auto& token : tokens_) {
    NotifyTokenObservers(token.first);
  }

  RecordNumAccountsMetric(tokens_.size());
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

void AccountManager::GetAccounts(AccountListCallback callback) {
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  base::OnceClosure closure =
      base::BindOnce(&AccountManager::GetAccountsInternal,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  RunOnInitialization(std::move(closure));
}

void AccountManager::GetAccountsInternal(AccountListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  std::vector<AccountKey> accounts = GetAccountKeys(tokens_);
  std::move(callback).Run(std::move(accounts));
}

void AccountManager::RemoveAccount(const AccountKey& account_key) {
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  base::OnceClosure closure =
      base::BindOnce(&AccountManager::RemoveAccountInternal,
                     weak_factory_.GetWeakPtr(), account_key);
  RunOnInitialization(std::move(closure));
}

void AccountManager::RemoveAccountInternal(const AccountKey& account_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  auto it = tokens_.find(account_key);
  if (it == tokens_.end()) {
    return;
  }

  MaybeRevokeTokenOnServer(account_key);
  tokens_.erase(it);
  PersistTokensAsync();
  NotifyAccountRemovalObservers(account_key);
}

void AccountManager::UpsertToken(const AccountKey& account_key,
                                 const std::string& token) {
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  if (account_key.account_type ==
      account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY) {
    DCHECK_EQ(token, kActiveDirectoryDummyToken);
  }

  base::OnceClosure closure =
      base::BindOnce(&AccountManager::UpsertTokenInternal,
                     weak_factory_.GetWeakPtr(), account_key, token);
  RunOnInitialization(std::move(closure));
}

void AccountManager::UpsertTokenInternal(const AccountKey& account_key,
                                         const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  DCHECK(account_key.IsValid()) << "Invalid account_key: " << account_key;

  auto it = tokens_.find(account_key);
  if ((it == tokens_.end()) || (it->second != token)) {
    MaybeRevokeTokenOnServer(account_key);
    tokens_[account_key] = token;
    PersistTokensAsync();
    NotifyTokenObservers(account_key);
  }
}

void AccountManager::PersistTokensAsync() {
  // Schedule (immediately) a non-blocking write.
  writer_->WriteNow(
      std::make_unique<std::string>(GetSerializedTokens(tokens_)));
}

void AccountManager::NotifyTokenObservers(const AccountKey& account_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_) {
    observer.OnTokenUpserted(account_key);
  }
}

void AccountManager::NotifyAccountRemovalObservers(
    const AccountKey& account_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_) {
    observer.OnAccountRemoved(account_key);
  }
}

void AccountManager::AddObserver(AccountManager::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void AccountManager::RemoveObserver(AccountManager::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

scoped_refptr<network::SharedURLLoaderFactory>
AccountManager::GetUrlLoaderFactory() {
  DCHECK(url_loader_factory_);
  return url_loader_factory_;
}

std::unique_ptr<OAuth2AccessTokenFetcher>
AccountManager::CreateAccessTokenFetcher(
    const AccountKey& account_key,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = tokens_.find(account_key);
  if (it == tokens_.end() || it->second.empty()) {
    return nullptr;
  }

  return std::make_unique<OAuth2AccessTokenFetcherImpl>(
      consumer, url_loader_factory, it->second);
}

bool AccountManager::IsTokenAvailable(const AccountKey& account_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = tokens_.find(account_key);
  return it != tokens_.end() && !it->second.empty() &&
         it->second != kActiveDirectoryDummyToken;
}

void AccountManager::MaybeRevokeTokenOnServer(const AccountKey& account_key) {
  auto it = tokens_.find(account_key);
  if (it == tokens_.end()) {
    return;
  }

  const std::string& token = it->second;

  if ((account_key.account_type ==
       account_manager::AccountType::ACCOUNT_TYPE_GAIA) &&
      !token.empty() && (token != kInvalidToken)) {
    RevokeGaiaTokenOnServer(token);
  }
}

void AccountManager::RevokeGaiaTokenOnServer(const std::string& refresh_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_token_revocation_requests_.emplace_back(
      std::make_unique<GaiaTokenRevocationRequest>(
          GetUrlLoaderFactory(), delay_network_call_runner_, refresh_token,
          weak_factory_.GetWeakPtr()));
}

void AccountManager::DeletePendingTokenRevocationRequest(
    GaiaTokenRevocationRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = std::find_if(
      pending_token_revocation_requests_.begin(),
      pending_token_revocation_requests_.end(),
      [&request](
          const std::unique_ptr<GaiaTokenRevocationRequest>& pending_request)
          -> bool { return pending_request.get() == request; });

  if (it != pending_token_revocation_requests_.end()) {
    pending_token_revocation_requests_.erase(it);
  }
}

CHROMEOS_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const AccountManager::AccountKey& account_key) {
  os << "{ id: " << account_key.id
     << ", account_type: " << account_key.account_type << " }";

  return os;
}

}  // namespace chromeos
