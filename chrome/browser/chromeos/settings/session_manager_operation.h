// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_SESSION_MANAGER_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_SESSION_MANAGER_OPERATION_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"

namespace enterprise_management {
class ChromeDeviceSettingsProto;
class PolicyData;
class PolicyFetchResponse;
}

namespace chromeos {

class OwnerKeyUtil;
class SessionManagerClient;

// Handles a single transaction with session manager. This is a virtual base
// class that contains common infrastructure for key and policy loading. There
// are subclasses for loading, storing and signing policy blobs.
class SessionManagerOperation {
 public:
  typedef base::Callback<void(SessionManagerOperation*,
                              DeviceSettingsService::Status)> Callback;

  // Creates a new load operation.
  explicit SessionManagerOperation(const Callback& callback);
  virtual ~SessionManagerOperation();

  // Starts the operation.
  void Start(SessionManagerClient* session_manager_client,
             scoped_refptr<OwnerKeyUtil> owner_key_util,
             scoped_refptr<OwnerKey> owner_key);

  // Restarts a load operation (if that part is already in progress).
  void RestartLoad(bool key_changed);

  // Accessors for recovering the loaded policy data after completion.
  scoped_ptr<enterprise_management::PolicyData>& policy_data() {
    return policy_data_;
  }
  scoped_ptr<enterprise_management::ChromeDeviceSettingsProto>&
      device_settings() {
    return device_settings_;
  }

  // Owner key as configured/loaded from disk.
  scoped_refptr<OwnerKey> owner_key() {
    return owner_key_;
  }

  // Whether the load operation is underway.
  bool is_loading() const {
    return is_loading_;
  }

  void set_force_key_load(bool force_key_load) {
    force_key_load_ = force_key_load;
  }

 protected:
  // Runs the operation. The result is reported through |callback_|.
  virtual void Run() = 0;

  // Ensures the owner key is loaded.
  void EnsureOwnerKey(const base::Closure& callback);

  // Starts a load operation.
  void StartLoading();

  // Reports the result status of the operation. Once this gets called, the
  // operation should not perform further processing or trigger callbacks.
  void ReportResult(DeviceSettingsService::Status status);

  SessionManagerClient* session_manager_client() {
    return session_manager_client_;
  }

 private:
  // Loads the owner key from disk. Must be run on a thread that can do I/O.
  static scoped_refptr<OwnerKey> LoadOwnerKey(
      scoped_refptr<OwnerKeyUtil> util,
      scoped_refptr<OwnerKey> current_key);

  // Stores the owner key loaded by LoadOwnerKey and calls |callback|.
  void StoreOwnerKey(const base::Closure& callback,
                     scoped_refptr<OwnerKey> new_key);

  // Triggers a device settings load.
  void RetrieveDeviceSettings();

  // Validates device settings after retrieval from session_manager.
  void ValidateDeviceSettings(const std::string& policy_blob);

  // Extracts status and device settings from the validator and reports them.
  void ReportValidatorStatus(policy::DeviceCloudPolicyValidator* validator);

  SessionManagerClient* session_manager_client_;
  scoped_refptr<OwnerKeyUtil> owner_key_util_;

  base::WeakPtrFactory<SessionManagerOperation> weak_factory_;

  Callback callback_;

  scoped_refptr<OwnerKey> owner_key_;
  bool force_key_load_;

  bool is_loading_;
  scoped_ptr<enterprise_management::PolicyData> policy_data_;
  scoped_ptr<enterprise_management::ChromeDeviceSettingsProto> device_settings_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerOperation);
};

// This operation loads the public owner key from disk if appropriate, fetches
// the policy blob from session manager, and validates the loaded policy blob.
class LoadSettingsOperation : public SessionManagerOperation {
 public:
  // Creates a new load operation.
  explicit LoadSettingsOperation(const Callback& callback);
  virtual ~LoadSettingsOperation();

 protected:
  // SessionManagerOperation:
  virtual void Run() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoadSettingsOperation);
};

// Stores a pre-generated policy blob and reloads the device settings from
// session_manager.
class StoreSettingsOperation : public SessionManagerOperation {
 public:
  // Creates a new store operation.
  StoreSettingsOperation(
      const Callback& callback,
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy);
  virtual ~StoreSettingsOperation();

 protected:
  // SessionManagerOperation:
  virtual void Run() OVERRIDE;

 private:
  // Handles the result of the store operation and triggers the load.
  void HandleStoreResult(bool success);

  scoped_ptr<enterprise_management::PolicyFetchResponse> policy_;

  base::WeakPtrFactory<StoreSettingsOperation> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StoreSettingsOperation);
};

// Signs device settings and stores the resulting blob to session_manager.
class SignAndStoreSettingsOperation : public SessionManagerOperation {
 public:
  // Creates a new sign-and-store operation.
  SignAndStoreSettingsOperation(
      const Callback& callback,
      scoped_ptr<enterprise_management::ChromeDeviceSettingsProto> new_settings,
      const std::string& username);
  virtual ~SignAndStoreSettingsOperation();

  // SessionManagerOperation:
  virtual void Run() OVERRIDE;

 private:
  // Given an owner key, starts the signing operation.
  void StartSigning();

  // Builds the policy blob and signs it using the owner key.
  static std::string AssembleAndSignPolicy(
      scoped_ptr<enterprise_management::ChromeDeviceSettingsProto> settings,
      const std::string& username,
      scoped_refptr<OwnerKey> owner_key);

  // Stores the signed device settings blob.
  void StoreDeviceSettingsBlob(std::string device_settings_blob);

  // Handles the result of the store operation and triggers the load.
  void HandleStoreResult(bool success);

  scoped_ptr<enterprise_management::ChromeDeviceSettingsProto> new_settings_;
  std::string username_;

  base::WeakPtrFactory<SignAndStoreSettingsOperation> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SignAndStoreSettingsOperation);
};

}  // namespace

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_SESSION_MANAGER_OPERATION_H_
