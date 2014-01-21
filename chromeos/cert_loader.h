// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CERT_LOADER_H_
#define CHROMEOS_CERT_LOADER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/tpm_token_loader.h"
#include "net/cert/cert_database.h"

namespace base {
class TaskRunner;
}

namespace net {
class X509Certificate;
}

namespace chromeos {

// This class is responsible for loading certificates once the TPM is
// initialized. It is expected to be constructed on the UI thread and public
// methods should all be called from the UI thread.
// When certificates have been loaded (after login completes and tpm token is
// initialized), or the cert database changes, observers are called with
// OnCertificatesLoaded().
// TODO(tbarzic): Remove direct dependency on TPMTokenLoader. The reason
//     TPMTokenLoader has to be observed is to make sure singleton NSS DB is
//     initialized before certificate loading starts. CertLoader should use
//     (primary) user specific NSS DB, whose loading already takes this into
//     account (crypto::GetPrivateSlotForChromeOSUser waits until TPM token is
//     ready).
class CHROMEOS_EXPORT CertLoader : public net::CertDatabase::Observer,
                                   public TPMTokenLoader::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the certificates, passed for convenience as |cert_list|,
    // have completed loading. |initial_load| is true the first time this
    // is called.
    virtual void OnCertificatesLoaded(const net::CertificateList& cert_list,
                                      bool initial_load) = 0;
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

  // Sets the task runner that any slow calls will be made from, e.g. calls
  // to the NSS database. If not set, uses base::WorkerPool.
  void SetSlowTaskRunnerForTest(
      const scoped_refptr<base::TaskRunner>& task_runner);

  void AddObserver(CertLoader::Observer* observer);
  void RemoveObserver(CertLoader::Observer* observer);

  // Returns true if the TPM is available for hardware-backed certificates.
  bool IsHardwareBacked() const;

  // Returns true when the certificate list has been requested but not loaded.
  bool CertificatesLoading() const;

  bool certificates_loaded() const { return certificates_loaded_; }

  // This will be empty until certificates_loaded() is true.
  const net::CertificateList& cert_list() const { return cert_list_; }

  // Getters for cached TPM token info.
  std::string tpm_user_pin() const { return tpm_user_pin_; }
  std::string tpm_token_name() const { return tpm_token_name_; }
  int tpm_token_slot_id() const { return tpm_token_slot_id_; }

 private:
  CertLoader();
  virtual ~CertLoader();

  // Starts certificate loading.
  void RequestCertificates();

  // Trigger a certificate load. If a certificate loading task is already in
  // progress, will start a reload once the current task finished.
  void LoadCertificates();

  // Called if a certificate load task is finished.
  void UpdateCertificates(net::CertificateList* cert_list);

  void NotifyCertificatesLoaded(bool initial_load);

  // net::CertDatabase::Observer
  virtual void OnCACertChanged(const net::X509Certificate* cert) OVERRIDE;
  virtual void OnCertAdded(const net::X509Certificate* cert) OVERRIDE;
  virtual void OnCertRemoved(const net::X509Certificate* cert) OVERRIDE;

  // chromeos::TPMTokenLoader::Observer
  virtual void OnTPMTokenReady(const std::string& tpm_user_pin,
                               const std::string& tpm_token_name,
                               int tpm_token_slot_id) OVERRIDE;

  ObserverList<Observer> observers_;

  // Flags describing current CertLoader state.
  bool certificates_requested_;
  bool certificates_loaded_;
  bool certificates_update_required_;
  bool certificates_update_running_;

  // Cached TPM token info. Set when the |OnTPMTokenReady| gets called.
  std::string tpm_user_pin_;
  std::string tpm_token_name_;
  int tpm_token_slot_id_;

  // Cached Certificates.
  net::CertificateList cert_list_;

  base::ThreadChecker thread_checker_;

  // TaskRunner for other slow tasks. May be set in tests.
  scoped_refptr<base::TaskRunner> slow_task_runner_for_test_;

  base::WeakPtrFactory<CertLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CertLoader);
};

}  // namespace chromeos

#endif  // CHROMEOS_CERT_LOADER_H_
