// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/session_manager_operation.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/tpm_token_loader.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/content_switches.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/signature_creator.h"

namespace em = enterprise_management;

using content::BrowserThread;
using ownership::OwnerKeyUtil;
using ownership::PrivateKey;
using ownership::PublicKey;

namespace chromeos {

namespace {

DeviceSettingsService* g_device_settings_service_for_testing = NULL;

bool IsOwnerInTests(const std::string& user_id) {
  if (user_id.empty() ||
      !CommandLine::ForCurrentProcess()->HasSwitch(::switches::kTestType) ||
      !CrosSettings::IsInitialized()) {
    return false;
  }
  const base::Value* value = CrosSettings::Get()->GetPref(kDeviceOwner);
  if (!value || value->GetType() != base::Value::TYPE_STRING)
    return false;
  return static_cast<const base::StringValue*>(value)->GetString() == user_id;
}

void LoadPrivateKeyByPublicKey(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    scoped_refptr<PublicKey> public_key,
    const std::string& username_hash,
    const base::Callback<void(const scoped_refptr<PublicKey>& public_key,
                              const scoped_refptr<PrivateKey>& private_key)>&
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

void LoadPrivateKey(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    const std::string username_hash,
    const base::Callback<void(const scoped_refptr<PublicKey>& public_key,
                              const scoped_refptr<PrivateKey>& private_key)>&
        callback) {
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
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    const OwnerSettingsServiceChromeOS::IsOwnerCallback& callback) {
  if (!owner_key_util.get()) {
    callback.Run(false);
    return;
  }
  scoped_refptr<base::TaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  base::PostTaskAndReplyWithResult(
      task_runner.get(),
      FROM_HERE,
      base::Bind(&DoesPrivateKeyExistAsyncHelper, owner_key_util),
      callback);
}

DeviceSettingsService* GetDeviceSettingsService() {
  if (g_device_settings_service_for_testing)
    return g_device_settings_service_for_testing;
  return DeviceSettingsService::IsInitialized() ? DeviceSettingsService::Get()
                                                : NULL;
}

}  // namespace

OwnerSettingsServiceChromeOS::OwnerSettingsServiceChromeOS(
    Profile* profile,
    const scoped_refptr<OwnerKeyUtil>& owner_key_util)
    : ownership::OwnerSettingsService(owner_key_util),
      profile_(profile),
      waiting_for_profile_creation_(true),
      waiting_for_tpm_token_(true),
      weak_factory_(this) {
  if (TPMTokenLoader::IsInitialized()) {
    TPMTokenLoader::TPMTokenStatus tpm_token_status =
        TPMTokenLoader::Get()->IsTPMTokenEnabled(
            base::Bind(&OwnerSettingsServiceChromeOS::OnTPMTokenReady,
                       weak_factory_.GetWeakPtr()));
    waiting_for_tpm_token_ =
        tpm_token_status == TPMTokenLoader::TPM_TOKEN_STATUS_UNDETERMINED;
  }

  if (DBusThreadManager::IsInitialized() &&
      DBusThreadManager::Get()->GetSessionManagerClient()) {
    DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);
  }

  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_CREATED,
                 content::Source<Profile>(profile_));
}

OwnerSettingsServiceChromeOS::~OwnerSettingsServiceChromeOS() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (DBusThreadManager::IsInitialized() &&
      DBusThreadManager::Get()->GetSessionManagerClient()) {
    DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
  }
}

void OwnerSettingsServiceChromeOS::OnTPMTokenReady(
    bool /* tpm_token_enabled */) {
  DCHECK(thread_checker_.CalledOnValidThread());
  waiting_for_tpm_token_ = false;

  // TPMTokenLoader initializes the TPM and NSS database which is necessary to
  // determine ownership. Force a reload once we know these are initialized.
  ReloadKeypair();
}

void OwnerSettingsServiceChromeOS::SignAndStorePolicyAsync(
    scoped_ptr<em::PolicyData> policy,
    const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SignAndStoreSettingsOperation* operation = new SignAndStoreSettingsOperation(
      base::Bind(&OwnerSettingsServiceChromeOS::HandleCompletedOperation,
                 weak_factory_.GetWeakPtr(),
                 callback),
      policy.Pass());
  operation->set_owner_settings_service(weak_factory_.GetWeakPtr());
  pending_operations_.push_back(operation);
  if (pending_operations_.front() == operation)
    StartNextOperation();
}

void OwnerSettingsServiceChromeOS::Observe(
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
  ReloadKeypair();
}

void OwnerSettingsServiceChromeOS::OwnerKeySet(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (success)
    ReloadKeypair();
}

// static
void OwnerSettingsServiceChromeOS::IsOwnerForSafeModeAsync(
    const std::string& user_hash,
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    const IsOwnerCallback& callback) {
  CHECK(chromeos::LoginState::Get()->IsInSafeMode());

  // Make sure NSS is initialized and NSS DB is loaded for the user before
  // searching for the owner key.
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&crypto::InitializeNSSForChromeOSUser),
                 user_hash,
                 ProfileHelper::GetProfilePathByUserIdHash(user_hash)),
      base::Bind(&DoesPrivateKeyExistAsync, owner_key_util, callback));
}

// static
void OwnerSettingsServiceChromeOS::SetDeviceSettingsServiceForTesting(
    DeviceSettingsService* device_settings_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_device_settings_service_for_testing = device_settings_service;
}

void OwnerSettingsServiceChromeOS::OnPostKeypairLoadedActions() {
  DCHECK(thread_checker_.CalledOnValidThread());

  user_id_ = profile_->GetProfileName();
  const bool is_owner = IsOwner() || IsOwnerInTests(user_id_);
  if (is_owner && GetDeviceSettingsService())
    GetDeviceSettingsService()->InitOwner(user_id_, weak_factory_.GetWeakPtr());
}

void OwnerSettingsServiceChromeOS::ReloadKeypairImpl(const base::Callback<
    void(const scoped_refptr<PublicKey>& public_key,
         const scoped_refptr<PrivateKey>& private_key)>& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (waiting_for_profile_creation_ || waiting_for_tpm_token_)
    return;
  scoped_refptr<base::TaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&LoadPrivateKey,
                 owner_key_util_,
                 ProfileHelper::GetUserIdHashFromProfile(profile_),
                 callback));
}

void OwnerSettingsServiceChromeOS::StartNextOperation() {
  DeviceSettingsService* service = GetDeviceSettingsService();
  if (!pending_operations_.empty() && service &&
      service->session_manager_client()) {
    pending_operations_.front()->Start(
        service->session_manager_client(), owner_key_util_, public_key_);
  }
}

void OwnerSettingsServiceChromeOS::HandleCompletedOperation(
    const base::Closure& callback,
    SessionManagerOperation* operation,
    DeviceSettingsService::Status status) {
  DCHECK_EQ(operation, pending_operations_.front());

  DeviceSettingsService* service = GetDeviceSettingsService();
  if (status == DeviceSettingsService::STORE_SUCCESS) {
    service->set_policy_data(operation->policy_data().Pass());
    service->set_device_settings(operation->device_settings().Pass());
  }

  if ((operation->public_key().get() && !public_key_.get()) ||
      (operation->public_key().get() && public_key_.get() &&
       operation->public_key()->data() != public_key_->data())) {
    // Public part changed so we need to reload private part too.
    ReloadKeypair();
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED,
        content::Source<OwnerSettingsServiceChromeOS>(this),
        content::NotificationService::NoDetails());
  }
  service->OnSignAndStoreOperationCompleted(status);
  if (!callback.is_null())
    callback.Run();

  pending_operations_.pop_front();
  delete operation;
  StartNextOperation();
}

}  // namespace chromeos
