// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"

#include "base/base64.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/state_store.h"
#include "net/cert/x509_certificate.h"

using content::BrowserThread;

namespace chromeos {

namespace {

const char kErrorKeyNotAllowedForSigning[] =
    "This key is not allowed for signing. Either it was used for signing "
    "before or it was not correctly generated.";
const char kStateStorePlatformKeys[] = "PlatformKeys";

scoped_ptr<base::StringValue> GetPublicKeyValue(
    const std::string& public_key_spki_der) {
  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);
  return make_scoped_ptr(new base::StringValue(public_key_spki_der_b64));
}

}  // namespace

class PlatformKeysService::Task {
 public:
  Task() {}
  virtual ~Task() {}
  virtual void Start() = 0;
  virtual bool IsDone() = 0;

 private:
  DISALLOW_ASSIGN(Task);
};

class PlatformKeysService::PermissionUpdateTask : public Task {
 public:
  enum class Step {
    READ_PLATFORM_KEYS,
    WRITE_UPDATE_AND_CALLBACK,
    DONE,
  };

  // Creates a task that reads the current permission for an extension to access
  // a certain key. Afterwards it updates and persists the permission to the new
  // value |new_permission_value|. |callback| will be run after the permission
  // was persisted. The old permission value is then accessible through
  // old_permission_value().
  PermissionUpdateTask(const bool new_permission_value,
                       const std::string& public_key_spki_der,
                       const std::string& extension_id,
                       base::Callback<void(Task*)> callback,
                       PlatformKeysService* service)
      : new_permission_value_(new_permission_value),
        public_key_spki_der_(public_key_spki_der),
        extension_id_(extension_id),
        callback_(callback),
        service_(service),
        weak_factory_(this) {}

  ~PermissionUpdateTask() override {}

  void Start() override {
    CHECK(next_step_ == Step::READ_PLATFORM_KEYS);
    DoStep();
  }

  bool IsDone() override { return next_step_ == Step::DONE; }

  // The original permission value before setting the new value
  // |new_permission_value|.
  bool old_permission_value() { return old_permission_value_; }

 private:
  void DoStep() {
    switch (next_step_) {
      case Step::READ_PLATFORM_KEYS:
        next_step_ = Step::WRITE_UPDATE_AND_CALLBACK;
        ReadPlatformKeys();
        return;
      case Step::WRITE_UPDATE_AND_CALLBACK:
        next_step_ = Step::DONE;
        WriteUpdate();
        if (!callback_.is_null()) {
          // Make a local copy of the callback to run as it might be deleted
          // during the Run().
          base::ResetAndReturn(&callback_).Run(this);
          // |this| might be invalid now.
        }
        return;
      case Step::DONE:
        NOTREACHED();
        return;
    }
  }

  // Reads the PlatformKeys value from the extension's state store and calls
  // back to GotPlatformKeys().
  void ReadPlatformKeys() {
    service_->GetPlatformKeysOfExtension(
        extension_id_, base::Bind(&PermissionUpdateTask::GotPlatformKeys,
                                  weak_factory_.GetWeakPtr()));
  }

  void GotPlatformKeys(scoped_ptr<base::ListValue> platform_keys) {
    platform_keys_ = platform_keys.Pass();
    DoStep();
  }

  // Returns whether the extension has permission to use the key for signing
  // according to the PlatformKeys value read from the extensions state store.
  // Invalidates the key if it was found to be valid.
  void WriteUpdate() {
    scoped_ptr<base::StringValue> key_value(
        GetPublicKeyValue(public_key_spki_der_));

    base::ListValue::const_iterator it = platform_keys_->Find(*key_value);
    old_permission_value_ = it != platform_keys_->end();
    if (old_permission_value_ == new_permission_value_)
      return;

    if (new_permission_value_)
      platform_keys_->Append(key_value.release());
    else
      platform_keys_->Remove(*key_value, nullptr);

    service_->SetPlatformKeysOfExtension(extension_id_, platform_keys_.Pass());
  }

  Step next_step_ = Step::READ_PLATFORM_KEYS;
  scoped_ptr<base::ListValue> platform_keys_;
  bool old_permission_value_ = false;

  const bool new_permission_value_;
  const std::string public_key_spki_der_;
  const std::string extension_id_;
  base::Callback<void(Task*)> callback_;
  PlatformKeysService* const service_;
  base::WeakPtrFactory<PermissionUpdateTask> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionUpdateTask);
};

class PlatformKeysService::SignTask : public Task {
 public:
  enum class Step {
    UPDATE_PERMISSION,
    SIGN_OR_ABORT,
    DONE,
  };

  // This Task will check the permissions of the extension with |extension_id|
  // for the key identified by |public_key_spki_der|, then updates the
  // permission to prevent any future signing operation of that extension using
  // that same key. If the permission check was positive, it will actually sign
  // |data| with the key and pass the signature to |callback|.
  // If an error occurs, an error message is passed to |callback| instead.
  SignTask(const std::string& token_id,
           const std::string& data,
           const std::string& public_key,
           bool sign_direct_pkcs_padded,
           platform_keys::HashAlgorithm hash_algorithm,
           const std::string& extension_id,
           const SignCallback& callback,
           PlatformKeysService* service)
      : token_id_(token_id),
        data_(data),
        public_key_(public_key),
        sign_direct_pkcs_padded_(sign_direct_pkcs_padded),
        hash_algorithm_(hash_algorithm),
        extension_id_(extension_id),
        callback_(callback),
        service_(service),
        weak_factory_(this) {}
  ~SignTask() override {}

  void Start() override {
    CHECK(next_step_ == Step::UPDATE_PERMISSION);
    DoStep();
  }
  bool IsDone() override { return next_step_ == Step::DONE; }

 private:
  void DoStep() {
    switch (next_step_) {
      case Step::UPDATE_PERMISSION:
        next_step_ = Step::SIGN_OR_ABORT;
        UpdatePermission();
        return;
      case Step::SIGN_OR_ABORT:
        next_step_ = Step::DONE;
        if (!service_->permission_check_enabled_ ||
            permission_update_->old_permission_value()) {
          Sign();
        } else {
          callback_.Run(std::string() /* no signature */,
                        kErrorKeyNotAllowedForSigning);
          DoStep();
        }
        return;
      case Step::DONE:
        service_->TaskFinished(this);
        // |this| might be invalid now.
        return;
    }
  }

  // Reads the current permission of the extension with |extension_id_| for key
  // |params_->public_key| and updates the permission to disable further
  // signing operations with that key.
  void UpdatePermission() {
    permission_update_.reset(new PermissionUpdateTask(
        false /* new permission value */, public_key_, extension_id_,
        base::Bind(&SignTask::DidUpdatePermission, weak_factory_.GetWeakPtr()),
        service_));
    permission_update_->Start();
  }

  void DidUpdatePermission(Task* /* task */) { DoStep(); }

  // Starts the actual signing operation and afterwards passes the signature (or
  // error) to |callback_|.
  void Sign() {
    if (sign_direct_pkcs_padded_) {
      platform_keys::subtle::SignRSAPKCS1Raw(
          token_id_, data_, public_key_,
          base::Bind(&SignTask::DidSign, weak_factory_.GetWeakPtr()),
          service_->browser_context_);
    } else {
      platform_keys::subtle::SignRSAPKCS1Digest(
          token_id_, data_, public_key_, hash_algorithm_,
          base::Bind(&SignTask::DidSign, weak_factory_.GetWeakPtr()),
          service_->browser_context_);
    }
  }

  void DidSign(const std::string& signature, const std::string& error_message) {
    callback_.Run(signature, error_message);
    DoStep();
  }

  Step next_step_ = Step::UPDATE_PERMISSION;
  scoped_ptr<base::ListValue> platform_keys_;
  scoped_ptr<PermissionUpdateTask> permission_update_;

  const std::string token_id_;
  const std::string data_;
  const std::string public_key_;

  // If true, |data_| will not be hashed before signing. Only PKCS#1 v1.5
  // padding will be applied before signing.
  // If false, |hash_algorithm_| is set to a value != NONE.
  const bool sign_direct_pkcs_padded_;
  const platform_keys::HashAlgorithm hash_algorithm_;
  const std::string extension_id_;
  const SignCallback callback_;
  PlatformKeysService* const service_;
  base::WeakPtrFactory<SignTask> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SignTask);
};

PlatformKeysService::PlatformKeysService(
    content::BrowserContext* browser_context,
    extensions::StateStore* state_store)
    : browser_context_(browser_context),
      state_store_(state_store),
      weak_factory_(this) {
  DCHECK(state_store);
}

PlatformKeysService::~PlatformKeysService() {
}

void PlatformKeysService::DisablePermissionCheckForTesting() {
  permission_check_enabled_ = false;
}

void PlatformKeysService::GenerateRSAKey(const std::string& token_id,
                                         unsigned int modulus_length,
                                         const std::string& extension_id,
                                         const GenerateKeyCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  platform_keys::subtle::GenerateRSAKey(
      token_id, modulus_length,
      base::Bind(&PlatformKeysService::GeneratedKey, weak_factory_.GetWeakPtr(),
                 extension_id, callback),
      browser_context_);
}

void PlatformKeysService::SignRSAPKCS1Digest(
    const std::string& token_id,
    const std::string& data,
    const std::string& public_key,
    platform_keys::HashAlgorithm hash_algorithm,
    const std::string& extension_id,
    const SignCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  StartOrQueueTask(make_scoped_ptr(new SignTask(
      token_id, data, public_key, false /* digest before signing */,
      hash_algorithm, extension_id, callback, this)));
}

void PlatformKeysService::SignRSAPKCS1Raw(const std::string& token_id,
                                          const std::string& data,
                                          const std::string& public_key,
                                          const std::string& extension_id,
                                          const SignCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  StartOrQueueTask(make_scoped_ptr(new SignTask(
      token_id, data, public_key, true /* sign directly without hashing */,
      platform_keys::HASH_ALGORITHM_NONE, extension_id, callback, this)));
}

void PlatformKeysService::SelectClientCertificates(
    const platform_keys::ClientCertificateRequest& request,
    const std::string& extension_id,
    const SelectCertificatesCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  platform_keys::subtle::SelectClientCertificates(
      request,
      base::Bind(&PlatformKeysService::SelectClientCertificatesCallback,
                 weak_factory_.GetWeakPtr(), extension_id, callback),
      browser_context_);
}

void PlatformKeysService::StartOrQueueTask(scoped_ptr<Task> task) {
  tasks_.push(make_linked_ptr(task.release()));
  if (tasks_.size() == 1)
    tasks_.front()->Start();
}

void PlatformKeysService::TaskFinished(Task* task) {
  DCHECK(!tasks_.empty());
  DCHECK(task == tasks_.front().get());
  // Remove all finished tasks from the queue (should be at most one).
  while (!tasks_.empty() && tasks_.front()->IsDone())
    tasks_.pop();

  // Now either the queue is empty or the next task is not finished yet and it
  // can be started.
  if (!tasks_.empty())
    tasks_.front()->Start();
}

void PlatformKeysService::GetPlatformKeysOfExtension(
    const std::string& extension_id,
    const GetPlatformKeysCallback& callback) {
  state_store_->GetExtensionValue(
      extension_id, kStateStorePlatformKeys,
      base::Bind(&PlatformKeysService::GotPlatformKeysOfExtension,
                 weak_factory_.GetWeakPtr(), extension_id, callback));
}

void PlatformKeysService::SetPlatformKeysOfExtension(
    const std::string& extension_id,
    scoped_ptr<base::ListValue> platform_keys) {
  state_store_->SetExtensionValue(extension_id, kStateStorePlatformKeys,
                                  platform_keys.Pass());
}

void PlatformKeysService::GeneratedKey(const std::string& extension_id,
                                       const GenerateKeyCallback& callback,
                                       const std::string& public_key_spki_der,
                                       const std::string& error_message) {
  if (!error_message.empty()) {
    callback.Run(std::string() /* no public key */, error_message);
    return;
  }

  StartOrQueueTask(make_scoped_ptr(new PermissionUpdateTask(
      true /* new permission value */, public_key_spki_der, extension_id,
      base::Bind(&PlatformKeysService::RegisteredGeneratedKey,
                 base::Unretained(this), callback, public_key_spki_der),
      this)));
}

void PlatformKeysService::RegisteredGeneratedKey(
    const GenerateKeyCallback& callback,
    const std::string& public_key_spki_der,
    Task* task) {
  callback.Run(public_key_spki_der, std::string() /* no error */);
  TaskFinished(task);
}

void PlatformKeysService::SelectClientCertificatesCallback(
    const std::string& extension_id,
    const SelectCertificatesCallback& callback,
    scoped_ptr<net::CertificateList> matches,
    const std::string& error_message) {
  if (permission_check_enabled_)
    matches->clear();

  // TODO(pneubeck): Remove all certs that the extension doesn't have access to.
  callback.Run(matches.Pass(), error_message);
}

void PlatformKeysService::GotPlatformKeysOfExtension(
    const std::string& extension_id,
    const GetPlatformKeysCallback& callback,
    scoped_ptr<base::Value> value) {
  if (!value)
    value.reset(new base::ListValue);

  base::ListValue* keys = NULL;
  if (!value->GetAsList(&keys)) {
    LOG(ERROR) << "Found a value of wrong type.";

    keys = new base::ListValue;
    value.reset(keys);
  }

  ignore_result(value.release());
  callback.Run(make_scoped_ptr(keys));
}

}  // namespace chromeos
