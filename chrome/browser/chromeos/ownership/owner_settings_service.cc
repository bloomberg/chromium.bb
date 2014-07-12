// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ownership/owner_settings_service.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/users/user.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/signature_creator.h"

namespace em = enterprise_management;

using content::BrowserThread;

namespace chromeos {

namespace {

scoped_refptr<OwnerKeyUtil>* g_owner_key_util_for_testing = NULL;
DeviceSettingsService* g_device_settings_service_for_testing = NULL;

std::string AssembleAndSignPolicy(scoped_ptr<em::PolicyData> policy,
                                  crypto::RSAPrivateKey* private_key) {
  // Assemble the policy.
  em::PolicyFetchResponse policy_response;
  if (!policy->SerializeToString(policy_response.mutable_policy_data())) {
    LOG(ERROR) << "Failed to encode policy payload.";
    return std::string();
  }

  // Generate the signature.
  scoped_ptr<crypto::SignatureCreator> signature_creator(
      crypto::SignatureCreator::Create(private_key));
  signature_creator->Update(
      reinterpret_cast<const uint8*>(policy_response.policy_data().c_str()),
      policy_response.policy_data().size());
  std::vector<uint8> signature_bytes;
  std::string policy_blob;
  if (!signature_creator->Final(&signature_bytes)) {
    LOG(ERROR) << "Failed to create policy signature.";
    return std::string();
  }

  policy_response.mutable_policy_data_signature()->assign(
      reinterpret_cast<const char*>(vector_as_array(&signature_bytes)),
      signature_bytes.size());
  return policy_response.SerializeAsString();
}

void LoadPrivateKeyByPublicKey(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    const std::vector<uint8>& public_key,
    const std::string& username_hash,
    const base::Callback<void(scoped_ptr<crypto::RSAPrivateKey>)>& callback) {
  crypto::EnsureNSSInit();
  crypto::ScopedPK11Slot slot =
      crypto::GetPublicSlotForChromeOSUser(username_hash);
  scoped_ptr<crypto::RSAPrivateKey> key(
      owner_key_util->FindPrivateKeyInSlot(public_key, slot.get()));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, base::Passed(&key)));
}

void LoadPrivateKey(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    const std::string username_hash,
    const base::Callback<void(scoped_ptr<crypto::RSAPrivateKey>)>& callback) {
  std::vector<uint8> public_key;
  if (!owner_key_util->ImportPublicKey(&public_key)) {
    scoped_ptr<crypto::RSAPrivateKey> result;
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(callback, base::Passed(&result)));
    return;
  }
  bool rv = BrowserThread::PostTask(BrowserThread::IO,
                                    FROM_HERE,
                                    base::Bind(&LoadPrivateKeyByPublicKey,
                                               owner_key_util,
                                               public_key,
                                               username_hash,
                                               callback));
  if (!rv) {
    // IO thread doesn't exists in unit tests, but it's safe to use NSS from
    // BlockingPool in unit tests.
    LoadPrivateKeyByPublicKey(
        owner_key_util, public_key, username_hash, callback);
  }
}

bool DoesPrivateKeyExistAsyncHelper(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util) {
  std::vector<uint8> public_key;
  if (!owner_key_util->ImportPublicKey(&public_key))
    return false;
  scoped_ptr<crypto::RSAPrivateKey> key(
      crypto::RSAPrivateKey::FindFromPublicKeyInfo(public_key));
  bool is_owner = key.get() != NULL;
  return is_owner;
}

// Checks whether NSS slots with private key are mounted or
// not. Responds via |callback|.
void DoesPrivateKeyExistAsync(
    const OwnerSettingsService::IsOwnerCallback& callback) {
  scoped_refptr<OwnerKeyUtil> owner_key_util;
  if (g_owner_key_util_for_testing)
    owner_key_util = *g_owner_key_util_for_testing;
  else
    owner_key_util = OwnerKeyUtil::Create();
  scoped_refptr<base::TaskRunner> task_runner =
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  base::PostTaskAndReplyWithResult(
      task_runner.get(),
      FROM_HERE,
      base::Bind(&DoesPrivateKeyExistAsyncHelper, owner_key_util),
      callback);
}

}  // namespace

OwnerSettingsService::OwnerSettingsService(Profile* profile)
    : profile_(profile),
      owner_key_util_(OwnerKeyUtil::Create()),
      waiting_for_profile_creation_(true),
      waiting_for_tpm_token_(true),
      weak_factory_(this) {
  if (TPMTokenLoader::IsInitialized()) {
    waiting_for_tpm_token_ = !TPMTokenLoader::Get()->IsTPMTokenReady();
    TPMTokenLoader::Get()->AddObserver(this);
  }

  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_CREATED,
                 content::Source<Profile>(profile_));
}

OwnerSettingsService::~OwnerSettingsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (TPMTokenLoader::IsInitialized())
    TPMTokenLoader::Get()->RemoveObserver(this);
}

bool OwnerSettingsService::IsOwner() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return private_key_ && private_key_->key();
}

void OwnerSettingsService::IsOwnerAsync(const IsOwnerCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (private_key_) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, IsOwner()));
  } else {
    pending_is_owner_callbacks_.push_back(callback);
  }
}

bool OwnerSettingsService::AssembleAndSignPolicyAsync(
    scoped_ptr<enterprise_management::PolicyData> policy,
    const AssembleAndSignPolicyCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsOwner())
    return false;
  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(
          &AssembleAndSignPolicy, base::Passed(&policy), private_key_->key()),
      callback);
  return true;
}

void OwnerSettingsService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (type != chrome::NOTIFICATION_PROFILE_CREATED) {
    NOTREACHED();
    return;
  }

  Profile* profile = content::Source<Profile>(source).ptr();
  if (profile != profile_) {
    NOTREACHED();
    return;
  }

  waiting_for_profile_creation_ = false;
  ReloadPrivateKey();
}

void OwnerSettingsService::OnTPMTokenReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  waiting_for_tpm_token_ = false;

  // TPMTokenLoader initializes the TPM and NSS database which is necessary to
  // determine ownership. Force a reload once we know these are initialized.
  ReloadPrivateKey();
}

// static
void OwnerSettingsService::IsOwnerForSafeModeAsync(
    const std::string& user_id,
    const std::string& user_hash,
    const IsOwnerCallback& callback) {
  CHECK(chromeos::LoginState::Get()->IsInSafeMode());

  // Make sure NSS is initialized and NSS DB is loaded for the user before
  // searching for the owner key.
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&crypto::InitializeNSSForChromeOSUser),
                 user_id,
                 user_hash,
                 ProfileHelper::GetProfilePathByUserIdHash(user_hash)),
      base::Bind(&DoesPrivateKeyExistAsync, callback));
}

// static
void OwnerSettingsService::SetOwnerKeyUtilForTesting(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (g_owner_key_util_for_testing) {
    delete g_owner_key_util_for_testing;
    g_owner_key_util_for_testing = NULL;
  }
  if (owner_key_util.get()) {
    g_owner_key_util_for_testing = new scoped_refptr<OwnerKeyUtil>();
    *g_owner_key_util_for_testing = owner_key_util;
  }
}

// static
void OwnerSettingsService::SetDeviceSettingsServiceForTesting(
    DeviceSettingsService* device_settings_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_device_settings_service_for_testing = device_settings_service;
}

void OwnerSettingsService::ReloadPrivateKey() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (waiting_for_profile_creation_ || waiting_for_tpm_token_)
    return;
  scoped_refptr<base::TaskRunner> task_runner =
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&LoadPrivateKey,
                 GetOwnerKeyUtil(),
                 ProfileHelper::GetUserIdHashFromProfile(profile_),
                 base::Bind(&OwnerSettingsService::OnPrivateKeyLoaded,
                            weak_factory_.GetWeakPtr())));
}

void OwnerSettingsService::OnPrivateKeyLoaded(
    scoped_ptr<crypto::RSAPrivateKey> private_key) {
  DCHECK(thread_checker_.CalledOnValidThread());
  private_key_ = new PrivateKey(private_key.release());

  const std::string& user_id = profile_->GetProfileName();
  if (user_id == OwnerSettingsServiceFactory::GetInstance()->GetUsername())
    GetDeviceSettingsService()->InitOwner(user_id, weak_factory_.GetWeakPtr());

  std::vector<IsOwnerCallback> is_owner_callbacks;
  is_owner_callbacks.swap(pending_is_owner_callbacks_);
  const bool is_owner = private_key_.get() && private_key_->key();
  for (std::vector<IsOwnerCallback>::iterator it(is_owner_callbacks.begin());
       it != is_owner_callbacks.end();
       ++it) {
    it->Run(is_owner);
  }
}

scoped_refptr<OwnerKeyUtil> OwnerSettingsService::GetOwnerKeyUtil() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (g_owner_key_util_for_testing)
    return *g_owner_key_util_for_testing;
  return owner_key_util_;
}

DeviceSettingsService* OwnerSettingsService::GetDeviceSettingsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (g_device_settings_service_for_testing)
    return g_device_settings_service_for_testing;
  if (DeviceSettingsService::IsInitialized())
    return DeviceSettingsService::Get();
  return NULL;
}

}  // namespace chromeos
