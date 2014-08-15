// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ownership/owner_settings_service.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/session_manager_operation.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
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

// Assembles PolicyData based on |settings|, |policy_data| and
// |user_id|.
scoped_ptr<em::PolicyData> AssemblePolicy(
    const std::string& user_id,
    const em::PolicyData* policy_data,
    const em::ChromeDeviceSettingsProto* settings) {
  scoped_ptr<em::PolicyData> policy(new em::PolicyData());
  if (policy_data) {
    // Preserve management settings.
    if (policy_data->has_management_mode())
      policy->set_management_mode(policy_data->management_mode());
    if (policy_data->has_request_token())
      policy->set_request_token(policy_data->request_token());
    if (policy_data->has_device_id())
      policy->set_device_id(policy_data->device_id());
  } else {
    // If there's no previous policy data, this is the first time the device
    // setting is set. We set the management mode to NOT_MANAGED initially.
    policy->set_management_mode(em::PolicyData::NOT_MANAGED);
  }
  policy->set_policy_type(policy::dm_protocol::kChromeDevicePolicyType);
  policy->set_timestamp(
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds());
  policy->set_username(user_id);
  if (!settings->SerializeToString(policy->mutable_policy_value()))
    return scoped_ptr<em::PolicyData>();

  return policy.Pass();
}

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
    scoped_refptr<PublicKey> public_key,
    const std::string& username_hash,
    const base::Callback<void(scoped_refptr<PublicKey> public_key,
                              scoped_refptr<PrivateKey> private_key)>&
        callback) {
  crypto::EnsureNSSInit();
  crypto::ScopedPK11Slot slot =
      crypto::GetPublicSlotForChromeOSUser(username_hash);
  scoped_refptr<PrivateKey> private_key(new PrivateKey(
      owner_key_util->FindPrivateKeyInSlot(public_key->data(), slot.get())));
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(callback, public_key, private_key));
}

void LoadPrivateKey(const scoped_refptr<OwnerKeyUtil>& owner_key_util,
                    const std::string username_hash,
                    const base::Callback<void(
                        scoped_refptr<PublicKey> public_key,
                        scoped_refptr<PrivateKey> private_key)>& callback) {
  std::vector<uint8> public_key_data;
  scoped_refptr<PublicKey> public_key;
  if (!owner_key_util->ImportPublicKey(&public_key_data)) {
    scoped_refptr<PrivateKey> private_key;
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(callback, public_key, private_key));
    return;
  }
  public_key = new PublicKey();
  public_key->data().swap(public_key_data);
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

// Returns the current management mode.
em::PolicyData::ManagementMode GetManagementMode(
    DeviceSettingsService* service) {
  if (!service) {
    LOG(ERROR) << "DeviceSettingsService is not initialized";
    return em::PolicyData::NOT_MANAGED;
  }

  const em::PolicyData* policy_data = service->policy_data();
  if (policy_data && policy_data->has_management_mode())
    return policy_data->management_mode();
  return em::PolicyData::NOT_MANAGED;
}

// Returns true if it is okay to transfer from the current mode to the new
// mode. This function should be called in SetManagementMode().
bool CheckManagementModeTransition(em::PolicyData::ManagementMode current_mode,
                                   em::PolicyData::ManagementMode new_mode) {
  // Mode is not changed.
  if (current_mode == new_mode)
    return true;

  switch (current_mode) {
    case em::PolicyData::NOT_MANAGED:
      // For consumer management enrollment.
      return new_mode == em::PolicyData::CONSUMER_MANAGED;

    case em::PolicyData::ENTERPRISE_MANAGED:
      // Management mode cannot be set when it is currently ENTERPRISE_MANAGED.
      return false;

    case em::PolicyData::CONSUMER_MANAGED:
      // For consumer management unenrollment.
      return new_mode == em::PolicyData::NOT_MANAGED;
  }

  NOTREACHED();
  return false;
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

  if (DBusThreadManager::IsInitialized() &&
      DBusThreadManager::Get()->GetSessionManagerClient()) {
    DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);
  }

  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_CREATED,
                 content::Source<Profile>(profile_));
}

OwnerSettingsService::~OwnerSettingsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (TPMTokenLoader::IsInitialized())
    TPMTokenLoader::Get()->RemoveObserver(this);

  if (DBusThreadManager::IsInitialized() &&
      DBusThreadManager::Get()->GetSessionManagerClient()) {
    DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
  }
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
    scoped_ptr<em::PolicyData> policy,
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

void OwnerSettingsService::SignAndStoreAsync(
    scoped_ptr<em::ChromeDeviceSettingsProto> settings,
    const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_ptr<em::PolicyData> policy = AssemblePolicy(
      user_id_, GetDeviceSettingsService()->policy_data(), settings.get());
  if (!policy) {
    HandleError(DeviceSettingsService::STORE_POLICY_ERROR, callback);
    return;
  }

  EnqueueSignAndStore(policy.Pass(), callback);
}

void OwnerSettingsService::SetManagementSettingsAsync(
    em::PolicyData::ManagementMode management_mode,
    const std::string& request_token,
    const std::string& device_id,
    const base::Closure& callback) {
  em::PolicyData::ManagementMode current_mode =
      GetManagementMode(GetDeviceSettingsService());
  if (!CheckManagementModeTransition(current_mode, management_mode)) {
    LOG(ERROR) << "Invalid management mode transition: current mode = "
               << current_mode << ", new mode = " << management_mode;
    HandleError(DeviceSettingsService::STORE_POLICY_ERROR, callback);
    return;
  }

  DeviceSettingsService* service = GetDeviceSettingsService();
  scoped_ptr<em::PolicyData> policy = AssemblePolicy(
      user_id_, service->policy_data(), service->device_settings());
  if (!policy) {
    HandleError(DeviceSettingsService::STORE_POLICY_ERROR, callback);
    return;
  }

  policy->set_management_mode(management_mode);
  policy->set_request_token(request_token);
  policy->set_device_id(device_id);

  EnqueueSignAndStore(policy.Pass(), callback);
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

void OwnerSettingsService::OwnerKeySet(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (success)
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
    scoped_refptr<PublicKey> public_key,
    scoped_refptr<PrivateKey> private_key) {
  DCHECK(thread_checker_.CalledOnValidThread());
  public_key_ = public_key;
  private_key_ = private_key;

  user_id_ = profile_->GetProfileName();
  if (user_id_ == OwnerSettingsServiceFactory::GetInstance()->GetUsername())
    GetDeviceSettingsService()->InitOwner(user_id_, weak_factory_.GetWeakPtr());

  std::vector<IsOwnerCallback> is_owner_callbacks;
  is_owner_callbacks.swap(pending_is_owner_callbacks_);
  const bool is_owner = IsOwner();
  for (std::vector<IsOwnerCallback>::iterator it(is_owner_callbacks.begin());
       it != is_owner_callbacks.end();
       ++it) {
    it->Run(is_owner);
  }
}

void OwnerSettingsService::EnqueueSignAndStore(
    scoped_ptr<em::PolicyData> policy,
    const base::Closure& callback) {
  SignAndStoreSettingsOperation* operation = new SignAndStoreSettingsOperation(
      base::Bind(&OwnerSettingsService::HandleCompletedOperation,
                 weak_factory_.GetWeakPtr(),
                 callback),
      policy.Pass());
  operation->set_delegate(weak_factory_.GetWeakPtr());
  pending_operations_.push_back(operation);
  if (pending_operations_.front() == operation)
    StartNextOperation();
}

void OwnerSettingsService::StartNextOperation() {
  DeviceSettingsService* service = GetDeviceSettingsService();
  if (!pending_operations_.empty() && service &&
      service->session_manager_client()) {
    pending_operations_.front()->Start(
        service->session_manager_client(), GetOwnerKeyUtil(), public_key_);
  }
}

void OwnerSettingsService::HandleCompletedOperation(
    const base::Closure& callback,
    SessionManagerOperation* operation,
    DeviceSettingsService::Status status) {
  DCHECK_EQ(operation, pending_operations_.front());

  DeviceSettingsService* service = GetDeviceSettingsService();
  if (status == DeviceSettingsService::STORE_SUCCESS) {
    service->set_policy_data(operation->policy_data().Pass());
    service->set_device_settings(operation->device_settings().Pass());
  }

  if ((operation->public_key() && !public_key_) ||
      (operation->public_key() && public_key_ &&
       operation->public_key()->data() != public_key_->data())) {
    // Public part changed so we need to reload private part too.
    ReloadPrivateKey();
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED,
        content::Source<OwnerSettingsService>(this),
        content::NotificationService::NoDetails());
  }
  service->OnSignAndStoreOperationCompleted(status);
  if (!callback.is_null())
    callback.Run();

  pending_operations_.pop_front();
  delete operation;
  StartNextOperation();
}

void OwnerSettingsService::HandleError(DeviceSettingsService::Status status,
                                       const base::Closure& callback) {
  LOG(ERROR) << "Session manager operation failed: " << status;
  GetDeviceSettingsService()->OnSignAndStoreOperationCompleted(status);
  if (!callback.is_null())
    callback.Run();
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
