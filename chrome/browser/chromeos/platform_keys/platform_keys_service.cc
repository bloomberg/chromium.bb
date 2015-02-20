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

struct PlatformKeysService::KeyEntry {
  // The base64-encoded DER of a X.509 Subject Public Key Info.
  std::string spki_b64;

  // True if the key can be used once for singing.
  // This permission is granted if an extension generated a key using the
  // enterprise.platformKeys API, so that it can build a certification request..
  // After the first signing operation this permission will be revoked.
  bool sign_once = false;

  // True if the key can be used for signing an unlimited number of times.
  // This permission is granted by the user or by policy to allow the extension
  // to use the key for signing through the enterprise.platformKeys or
  // platformKeys API.
  // This permission is granted until revoked by the user or the policy.
  bool sign_unlimited = false;
};

namespace {

const char kErrorKeyNotAllowedForSigning[] =
    "This key is not allowed for signing. Either it was used for signing "
    "before or it was not correctly generated.";

// The key at which platform key specific data is stored in each extension's
// state store.
// From older versions of ChromeOS, this key can hold a list of base64 and
// DER-encoded SPKIs. A key can be used for signing at most once if it is part
// of that list
// and removed from that list afterwards.
//
// The current format of data that is written to the PlatformKeys field is a
// list of serialized KeyEntry objects:
//   { 'SPKI': string,
//     'signOnce': bool,  // if not present, defaults to false
//     'signUnlimited': bool  // if not present, defaults to false
//   }
//
// Do not change this constant as clients will lose their existing state.
const char kStateStorePlatformKeys[] = "PlatformKeys";
const char kStateStoreSPKI[] = "SPKI";
const char kStateStoreSignOnce[] = "signOnce";
const char kStateStoreSignUnlimited[] = "signUnlimited";

scoped_ptr<PlatformKeysService::KeyEntries> KeyEntriesFromState(
    const base::Value& state) {
  scoped_ptr<PlatformKeysService::KeyEntries> new_entries(
      new PlatformKeysService::KeyEntries);

  const base::ListValue* entries = nullptr;
  if (!state.GetAsList(&entries)) {
    LOG(ERROR) << "Found a state store of wrong type.";
    return new_entries.Pass();
  }
  for (const base::Value* entry : *entries) {
    if (!entry) {
      LOG(ERROR) << "Found invalid NULL entry in PlatformKeys state store.";
      continue;
    }

    PlatformKeysService::KeyEntry new_entry;
    const base::DictionaryValue* dict_entry = nullptr;
    if (entry->GetAsString(&new_entry.spki_b64)) {
      // This handles the case that the store contained a plain list of base64
      // and DER-encoded SPKIs from an older version of ChromeOS.
      new_entry.sign_once = true;
    } else if (entry->GetAsDictionary(&dict_entry)) {
      dict_entry->GetStringWithoutPathExpansion(kStateStoreSPKI,
                                                &new_entry.spki_b64);
      dict_entry->GetBooleanWithoutPathExpansion(kStateStoreSignOnce,
                                                 &new_entry.sign_once);
      dict_entry->GetBooleanWithoutPathExpansion(kStateStoreSignUnlimited,
                                                 &new_entry.sign_unlimited);
    } else {
      LOG(ERROR) << "Found invalid entry of type " << entry->GetType()
                 << " in PlatformKeys state store.";
      continue;
    }
    new_entries->push_back(new_entry);
  }
  return new_entries.Pass();
}

scoped_ptr<base::ListValue> KeyEntriesToState(
    const PlatformKeysService::KeyEntries& entries) {
  scoped_ptr<base::ListValue> new_state(new base::ListValue);
  for (const PlatformKeysService::KeyEntry& entry : entries) {
    // Drop entries that the extension doesn't have any permissions for anymore.
    if (!entry.sign_once && !entry.sign_unlimited)
      continue;

    scoped_ptr<base::DictionaryValue> new_entry(new base::DictionaryValue);
    new_entry->SetStringWithoutPathExpansion(kStateStoreSPKI, entry.spki_b64);
    // Omit writing default values, namely |false|.
    if (entry.sign_once) {
      new_entry->SetBooleanWithoutPathExpansion(kStateStoreSignOnce,
                                                entry.sign_once);
    }
    if (entry.sign_unlimited) {
      new_entry->SetBooleanWithoutPathExpansion(kStateStoreSignUnlimited,
                                                entry.sign_unlimited);
    }
    new_state->Append(new_entry.release());
  }
  return new_state.Pass();
}

// Searches |platform_keys| for an entry for |public_key_spki_der_b64|. If found
// returns a pointer to it, otherwise returns null.
PlatformKeysService::KeyEntry* GetMatchingEntry(
    const std::string& public_key_spki_der_b64,
    PlatformKeysService::KeyEntries* platform_keys) {
  for (PlatformKeysService::KeyEntry& entry : *platform_keys) {
    // For every ASN.1 value there is exactly one DER encoding, so it is fine to
    // compare the DER (or its base64 encoding).
    if (entry.spki_b64 == public_key_spki_der_b64)
      return &entry;
  }
  return nullptr;
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
  // was persisted. The old permission values are then available through
  // old_key_entry().
  PermissionUpdateTask(const SignPermission permission,
                       const bool new_permission_value,
                       const std::string& public_key_spki_der,
                       const std::string& extension_id,
                       base::Callback<void(Task*)> callback,
                       PlatformKeysService* service)
      : permission_(permission),
        new_permission_value_(new_permission_value),
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

  // The old key entry with the old permissions before setting |permission| to
  // the new value |new_permission_value|.
  const KeyEntry& old_key_entry() { return old_key_entry_; }

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

  void GotPlatformKeys(scoped_ptr<KeyEntries> platform_keys) {
    platform_keys_ = platform_keys.Pass();
    DoStep();
  }

  // Persists the existing KeyEntry in |old_key_entry_|, updates the entry with
  // the new permission and persists it to the extension's state store if it was
  // changed.
  void WriteUpdate() {
    DCHECK(platform_keys_);

    std::string public_key_spki_der_b64;
    base::Base64Encode(public_key_spki_der_, &public_key_spki_der_b64);

    KeyEntry* matching_entry =
        GetMatchingEntry(public_key_spki_der_b64, platform_keys_.get());

    if (!matching_entry) {
      platform_keys_->push_back(KeyEntry());
      matching_entry = &platform_keys_->back();
      matching_entry->spki_b64 = public_key_spki_der_b64;
    } else if (permission_ == SignPermission::ONCE && new_permission_value_) {
      // The one-time sign permission is supposed to be granted once per key
      // during generation. Generated keys should be unique and thus this case
      // should never occur.
      NOTREACHED() << "Requested one-time sign permission on existing key.";
    }
    old_key_entry_ = *matching_entry;

    bool* permission_value = nullptr;
    switch (permission_) {
      case SignPermission::ONCE:
        permission_value = &matching_entry->sign_once;
        break;
      case SignPermission::UNLIMITED:
        permission_value = &matching_entry->sign_unlimited;
        break;
    }

    if (*permission_value != new_permission_value_) {
      *permission_value = new_permission_value_;
      service_->SetPlatformKeysOfExtension(extension_id_, *platform_keys_);
    }
  }

  Step next_step_ = Step::READ_PLATFORM_KEYS;
  KeyEntry old_key_entry_;

  const SignPermission permission_;
  const bool new_permission_value_;
  const std::string public_key_spki_der_;
  const std::string extension_id_;
  scoped_ptr<KeyEntries> platform_keys_;
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
      case Step::SIGN_OR_ABORT: {
        next_step_ = Step::DONE;
        bool sign_granted = permission_update_->old_key_entry().sign_once ||
                            permission_update_->old_key_entry().sign_unlimited;
        if (sign_granted) {
          Sign();
        } else {
          if (!callback_.is_null()) {
            callback_.Run(std::string() /* no signature */,
                          kErrorKeyNotAllowedForSigning);
          }
          DoStep();
        }
        return;
      }
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
        SignPermission::ONCE, false /* new permission value */, public_key_,
        extension_id_,
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
  scoped_ptr<KeyEntries> platform_keys_;
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

class PlatformKeysService::SelectTask : public Task {
 public:
  enum class Step {
    GET_MATCHING_CERTS,
    SELECT_CERTS,
    READ_PLATFORM_KEYS,
    UPDATE_PERMISSION,
    FILTER_BY_PERMISSIONS,
    DONE,
  };

  // This task determines all known client certs matching |request|. If
  // |interactive| is true, calls |service->select_delegate_->Select()| to
  // select a cert from all matches. The extension with |extension_id| will be
  // granted unlimited sign permission for the selected cert.
  // Finally, either the selection or, if |interactive| is false, matching certs
  // that the extension has permission for are passed to |callback|.
  SelectTask(const platform_keys::ClientCertificateRequest& request,
             bool interactive,
             const std::string& extension_id,
             const SelectCertificatesCallback& callback,
             content::WebContents* web_contents,
             PlatformKeysService* service)
      : request_(request),
        interactive_(interactive),
        extension_id_(extension_id),
        callback_(callback),
        web_contents_(web_contents),
        service_(service),
        weak_factory_(this) {}
  ~SelectTask() override {}

  void Start() override {
    CHECK(next_step_ == Step::GET_MATCHING_CERTS);
    DoStep();
  }
  bool IsDone() override { return next_step_ == Step::DONE; }

 private:
  void DoStep() {
    switch (next_step_) {
      case Step::GET_MATCHING_CERTS:
        if (interactive_)
          next_step_ = Step::SELECT_CERTS;
        else  // Skip SelectCerts and UpdatePermission if not interactive.
          next_step_ = Step::READ_PLATFORM_KEYS;
        GetMatchingCerts();
        return;
      case Step::SELECT_CERTS:
        next_step_ = Step::UPDATE_PERMISSION;
        SelectCerts();
        return;
      case Step::UPDATE_PERMISSION:
        next_step_ = Step::READ_PLATFORM_KEYS;
        UpdatePermission();
        return;
      case Step::READ_PLATFORM_KEYS:
        next_step_ = Step::FILTER_BY_PERMISSIONS;
        ReadPlatformKeys();
        return;
      case Step::FILTER_BY_PERMISSIONS:
        next_step_ = Step::DONE;
        FilterSelectionByPermission();
        return;
      case Step::DONE:
        service_->TaskFinished(this);
        // |this| might be invalid now.
        return;
    }
  }

  // Retrieves all certificates matching |request_|. Will call back to
  // |GotMatchingCerts()|.
  void GetMatchingCerts() {
    platform_keys::subtle::SelectClientCertificates(
        request_,
        base::Bind(&SelectTask::GotMatchingCerts, weak_factory_.GetWeakPtr()),
        service_->browser_context_);
  }

  // If the certificate request could be processed successfully, |matches| will
  // contain the list of matching certificates (maybe empty) and |error_message|
  // will be empty. If an error occurred, |matches| will be null and
  // |error_message| contain an error message.
  void GotMatchingCerts(scoped_ptr<net::CertificateList> matches,
                        const std::string& error_message) {
    if (!error_message.empty()) {
      next_step_ = Step::DONE;
      callback_.Run(nullptr /* no certificates */, error_message);
      DoStep();
      return;
    }
    matches_.swap(*matches);
    DoStep();
  }

  // Calls |service_->select_delegate_->Select()| to select a cert from
  // |matches_|, which will be stored in |selected_cert_|.
  // Will call back to |GotSelection()|.
  void SelectCerts() {
    CHECK(interactive_);
    if (matches_.empty()) {
      // Don't show a select dialog if no certificate is matching.
      DoStep();
      return;
    }
    service_->select_delegate_->Select(
        extension_id_, matches_,
        base::Bind(&SelectTask::GotSelection, base::Unretained(this)),
        web_contents_, service_->browser_context_);
  }

  // Will be called by |SelectCerts()| with the selected cert or null if no cert
  // was selected.
  void GotSelection(const scoped_refptr<net::X509Certificate>& selected_cert) {
    selected_cert_ = selected_cert;
    DoStep();
  }

  // Updates the extension's state store about unlimited sign permission for the
  // selected cert. Does nothing if no cert was selected.
  // Will call back to |DidUpdatePermission()|.
  void UpdatePermission() {
    CHECK(interactive_);
    if (!selected_cert_) {
      DoStep();
      return;
    }
    const std::string public_key_spki_der(
        platform_keys::GetSubjectPublicKeyInfo(selected_cert_));
    permission_update_.reset(new PermissionUpdateTask(
        SignPermission::UNLIMITED, true /* new permission value */,
        public_key_spki_der, extension_id_,
        base::Bind(&SelectTask::DidUpdatePermission, base::Unretained(this)),
        service_));
    permission_update_->Start();
  }

  void DidUpdatePermission(Task* /* task */) { DoStep(); }

  // Reads the PlatformKeys value from the extension's state store and calls
  // back to GotPlatformKeys().
  void ReadPlatformKeys() {
    service_->GetPlatformKeysOfExtension(
        extension_id_,
        base::Bind(&SelectTask::GotPlatformKeys, weak_factory_.GetWeakPtr()));
  }

  void GotPlatformKeys(scoped_ptr<KeyEntries> platform_keys) {
    platform_keys_ = platform_keys.Pass();
    DoStep();
  }

  // Filters from all matches (if not interactive) or from the selection (if
  // interactive), the certificates that the extension has unlimited sign
  // permission for. Passes the filtered certs to |callback_|.
  void FilterSelectionByPermission() {
    scoped_ptr<net::CertificateList> selection(new net::CertificateList);
    if (interactive_) {
      if (selected_cert_)
        selection->push_back(selected_cert_);
    } else {
      selection->assign(matches_.begin(), matches_.end());
    }

    scoped_ptr<net::CertificateList> filtered_certs(new net::CertificateList);
    for (scoped_refptr<net::X509Certificate> selected_cert : *selection) {
      const std::string public_key_spki_der(
          platform_keys::GetSubjectPublicKeyInfo(selected_cert));
      std::string public_key_spki_der_b64;
      base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

      KeyEntry* matching_entry =
          GetMatchingEntry(public_key_spki_der_b64, platform_keys_.get());
      if (!matching_entry || !matching_entry->sign_unlimited)
        continue;

      filtered_certs->push_back(selected_cert);
    }
    // Note: In the interactive case this should have filtered exactly the
    // one selected cert. Checking the permissions again is not striclty
    // necessary but this ensures that the permissions were updated correctly.
    CHECK(!selected_cert_ || (filtered_certs->size() == 1 &&
                              filtered_certs->front() == selected_cert_));
    callback_.Run(filtered_certs.Pass(), std::string() /* no error */);
    DoStep();
  }

  Step next_step_ = Step::GET_MATCHING_CERTS;
  scoped_ptr<KeyEntries> platform_keys_;
  scoped_ptr<PermissionUpdateTask> permission_update_;

  net::CertificateList matches_;
  scoped_refptr<net::X509Certificate> selected_cert_;
  platform_keys::ClientCertificateRequest request_;
  const bool interactive_;
  const std::string extension_id_;
  const SelectCertificatesCallback callback_;
  content::WebContents* const web_contents_;
  PlatformKeysService* const service_;
  base::WeakPtrFactory<SelectTask> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SelectTask);
};

PlatformKeysService::SelectDelegate::SelectDelegate() {
}

PlatformKeysService::SelectDelegate::~SelectDelegate() {
}

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

void PlatformKeysService::SetSelectDelegate(
    scoped_ptr<SelectDelegate> delegate) {
  select_delegate_ = delegate.Pass();
}

void PlatformKeysService::GrantUnlimitedSignPermission(
    const std::string& extension_id,
    scoped_refptr<net::X509Certificate> cert) {
  const std::string public_key_spki_der(
      platform_keys::GetSubjectPublicKeyInfo(cert));

  StartOrQueueTask(make_scoped_ptr(new PermissionUpdateTask(
      SignPermission::UNLIMITED, true /* new permission value */,
      public_key_spki_der, extension_id,
      base::Bind(&PlatformKeysService::TaskFinished, base::Unretained(this)),
      this)));
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
    bool interactive,
    const std::string& extension_id,
    const SelectCertificatesCallback& callback,
    content::WebContents* web_contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  StartOrQueueTask(make_scoped_ptr(new SelectTask(
      request, interactive, extension_id, callback, web_contents, this)));
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
    const KeyEntries& platform_keys) {
  state_store_->SetExtensionValue(extension_id, kStateStorePlatformKeys,
                                  KeyEntriesToState(platform_keys));
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
      SignPermission::ONCE, true /* new permission value */,
      public_key_spki_der, extension_id,
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


void PlatformKeysService::GotPlatformKeysOfExtension(
    const std::string& extension_id,
    const GetPlatformKeysCallback& callback,
    scoped_ptr<base::Value> value) {
  scoped_ptr<KeyEntries> key_entries(new KeyEntries);
  if (value)
    key_entries = KeyEntriesFromState(*value);

  callback.Run(key_entries.Pass());
}

}  // namespace chromeos
