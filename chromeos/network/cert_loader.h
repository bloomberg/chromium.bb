// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CERT_LOADER_H_
#define CHROMEOS_NETWORK_CERT_LOADER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/threading/thread_checker.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/login/login_state.h"
#include "net/cert/cert_database.h"
#include "net/cert/x509_certificate.h"

namespace crypto {
class SymmetricKey;
}

namespace chromeos {

// This class is responsible for initializing the TPM token and loading
// certificates once the TPM is initialized. It is expected to be constructed
// on the UI thread and public methods should all be called from the UI thread.
// When certificates have been loaded (after login completes), or the cert
// database changes, observers are called with OnCertificatesLoaded().
class CHROMEOS_EXPORT CertLoader : public net::CertDatabase::Observer,
                                   public LoginState::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the certificates, passed for convenience as |cert_list|,
    // have completed loading. |initial_load| is true the first time this
    // is called.
    virtual void OnCertificatesLoaded(const net::CertificateList& cert_list,
                                      bool initial_load) = 0;

   protected:
    Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Manage the global instance.
  static void Initialize();
  static void Shutdown();
  static CertLoader* Get();
  static bool IsInitialized();

  void AddObserver(CertLoader::Observer* observer);
  void RemoveObserver(CertLoader::Observer* observer);

  // Returns true when the certificate list has been requested but not loaded.
  bool CertificatesLoading() const;

  // Returns true if the TPM is available for hardware-backed certificates.
  bool IsHardwareBacked() const;

  bool certificates_loaded() const { return certificates_loaded_; }

  // TPM info is only valid once the TPM is available (IsHardwareBacked is
  // true). Otherwise empty strings will be returned.
  const std::string& tpm_token_name() const { return tpm_token_name_; }
  const std::string& tpm_token_slot() const { return tpm_token_slot_; }
  const std::string& tpm_user_pin() const { return tpm_user_pin_; }

  // This will be empty until certificates_loaded() is true.
  const net::CertificateList& cert_list() const { return cert_list_; }

 private:
  CertLoader();
  virtual ~CertLoader();

  void RequestCertificates();

  void OnTpmIsEnabled(DBusMethodCallStatus call_status,
                      bool tpm_is_enabled);
  void OnPkcs11IsTpmTokenReady(DBusMethodCallStatus call_status,
                               bool is_tpm_token_ready);
  void OnPkcs11GetTpmTokenInfo(DBusMethodCallStatus call_status,
                               const std::string& token_name,
                               const std::string& user_pin);
  void InitializeTPMToken();
  void StartLoadCertificates();
  void UpdateCertificates(net::CertificateList* cert_list);
  void MaybeRetryRequestCertificates();
  void RequestCertificatesTask();

  void NotifyCertificatesLoaded(bool initial_load);

  // net::CertDatabase::Observer
  virtual void OnCertTrustChanged(const net::X509Certificate* cert) OVERRIDE;
  virtual void OnCertAdded(const net::X509Certificate* cert) OVERRIDE;
  virtual void OnCertRemoved(const net::X509Certificate* cert) OVERRIDE;

  // LoginState::Observer
  virtual void LoggedInStateChanged(LoginState::LoggedInState state) OVERRIDE;

  ObserverList<Observer> observers_;

  // Active request task for re-requests while waiting for TPM init.
  base::Closure request_task_;

  // Local state.
  bool tpm_token_ready_;
  bool certificates_requested_;
  bool certificates_loaded_;
  // The key store for the current user has been loaded. This flag is needed to
  // ensure that the key store will not be loaded twice in the policy recovery
  // "safe-mode".
  bool key_store_loaded_;

  // Cached TPM token info.
  std::string tpm_token_name_;
  std::string tpm_token_slot_;
  std::string tpm_user_pin_;

  // Cached Certificates.
  net::CertificateList cert_list_;

  base::ThreadChecker thread_checker_;

  // TODO(stevenjb): Use multiple factories to track callback chains.
  base::WeakPtrFactory<CertLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CertLoader);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CERT_LOADER_H_
