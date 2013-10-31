// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CERT_LOADER_H_
#define CHROMEOS_CERT_LOADER_H_

#include <string>

#include "base/basictypes.h"
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

namespace base {
class SequencedTaskRunner;
class TaskRunner;
}

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

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static CertLoader* Get();

  // Returns true if the global instance has been initialized.
  static bool IsInitialized();

  static std::string GetPkcs11IdForCert(const net::X509Certificate& cert);

  // By default, CertLoader tries to load the TPMToken only if running in a
  // ChromeOS environment. Tests can call this function after Initialize() and
  // before SetCryptoTaskRunner() to enable the TPM initialization.
  void InitializeTPMForTest();

  // |crypto_task_runner| is the task runner that any synchronous crypto calls
  // should be made from, e.g. in Chrome this is the IO thread. Must be called
  // after the thread is started. Starts TPM initialization and Certificate
  // loading.
  void SetCryptoTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner>& crypto_task_runner);

  // Sets the task runner that any slow calls will be made from, e.g. calls
  // to the NSS database. If not set, uses base::WorkerPool.
  void SetSlowTaskRunnerForTest(
      const scoped_refptr<base::TaskRunner>& task_runner);

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
  int tpm_token_slot_id() const { return tpm_token_slot_id_; }
  const std::string& tpm_user_pin() const { return tpm_user_pin_; }

  // This will be empty until certificates_loaded() is true.
  const net::CertificateList& cert_list() const { return cert_list_; }

 private:
  CertLoader();
  virtual ~CertLoader();

  void MaybeRequestCertificates();

  // This is the cyclic chain of callbacks to initialize the TPM token and to
  // kick off the update of the certificate list.
  void InitializeTokenAndLoadCertificates();
  void RetryTokenInitializationLater();
  void OnPersistentNSSDBOpened();
  void OnTpmIsEnabled(DBusMethodCallStatus call_status,
                      bool tpm_is_enabled);
  void OnPkcs11IsTpmTokenReady(DBusMethodCallStatus call_status,
                               bool is_tpm_token_ready);
  void OnPkcs11GetTpmTokenInfo(DBusMethodCallStatus call_status,
                               const std::string& token_name,
                               const std::string& user_pin,
                               int token_slot_id);
  void OnTPMTokenInitialized(bool success);

  // These calls handle the updating of the certificate list after the TPM token
  // was initialized.

  // Start certificate loading. Must be called at most once.
  void StartLoadCertificates();

  // Trigger a certificate load. If a certificate loading task is already in
  // progress, will start a reload once the current task finised.
  void LoadCertificates();

  // Called if a certificate load task is finished.
  void UpdateCertificates(net::CertificateList* cert_list);

  void NotifyCertificatesLoaded(bool initial_load);

  // net::CertDatabase::Observer
  virtual void OnCACertChanged(const net::X509Certificate* cert) OVERRIDE;
  virtual void OnCertAdded(const net::X509Certificate* cert) OVERRIDE;
  virtual void OnCertRemoved(const net::X509Certificate* cert) OVERRIDE;

  // LoginState::Observer
  virtual void LoggedInStateChanged() OVERRIDE;

  bool initialize_tpm_for_test_;

  ObserverList<Observer> observers_;

  bool certificates_requested_;
  bool certificates_loaded_;
  bool certificates_update_required_;
  bool certificates_update_running_;

  // The states are traversed in this order but some might get omitted or never
  // be left.
  enum TPMTokenState {
    TPM_STATE_UNKNOWN,
    TPM_DB_OPENED,
    TPM_DISABLED,
    TPM_ENABLED,
    TPM_TOKEN_READY,
    TPM_TOKEN_INFO_RECEIVED,
    TPM_TOKEN_INITIALIZED,
  };
  TPMTokenState tpm_token_state_;

  // The current request delay before the next attempt to initialize the
  // TPM. Will be adapted after each attempt.
  base::TimeDelta tpm_request_delay_;

  // Cached TPM token info.
  std::string tpm_token_name_;
  int tpm_token_slot_id_;
  std::string tpm_user_pin_;

  // Cached Certificates.
  net::CertificateList cert_list_;

  base::ThreadChecker thread_checker_;

  // TaskRunner for crypto calls.
  scoped_refptr<base::SequencedTaskRunner> crypto_task_runner_;

  // TaskRunner for other slow tasks. May be set in tests.
  scoped_refptr<base::TaskRunner> slow_task_runner_for_test_;

  // This factory should be used only for callbacks during TPMToken
  // initialization.
  base::WeakPtrFactory<CertLoader> initialize_token_factory_;

  // This factory should be used only for callbacks during updating the
  // certificate list.
  base::WeakPtrFactory<CertLoader> update_certificates_factory_;

  DISALLOW_COPY_AND_ASSIGN(CertLoader);
};

}  // namespace chromeos

#endif  // CHROMEOS_CERT_LOADER_H_
