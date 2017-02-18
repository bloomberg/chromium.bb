// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CERT_LOADER_H_
#define CHROMEOS_CERT_LOADER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chromeos/chromeos_export.h"
#include "net/cert/cert_database.h"

namespace net {
class NSSCertDatabase;
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace chromeos {

// This class is responsible for loading certificates once the TPM is
// initialized. It is expected to be constructed on the UI thread and public
// methods should all be called from the UI thread.
// When certificates have been loaded (after login completes and tpm token is
// initialized), or the cert database changes, observers are called with
// OnCertificatesLoaded().
class CHROMEOS_EXPORT CertLoader : public net::CertDatabase::Observer {
 public:
  class Observer {
   public:
    // Called when the certificates, passed for convenience as |cert_list|,
    // have completed loading. |initial_load| is true the first time this
    // is called.
    virtual void OnCertificatesLoaded(const net::CertificateList& cert_list,
                                      bool initial_load) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static CertLoader* Get();

  // Returns true if the global instance has been initialized.
  static bool IsInitialized();

  // Returns the PKCS#11 attribute CKA_ID for a certificate as an upper-case
  // hex string and sets |slot_id| to the id of the containing slot, or returns
  // an empty string and doesn't modify |slot_id| if the PKCS#11 id could not be
  // determined.
  static std::string GetPkcs11IdAndSlotForCert(const net::X509Certificate& cert,
                                               int* slot_id);

  // Starts the CertLoader with the NSS cert database.
  // The CertLoader will _not_ take the ownership of the database, but it
  // expects it to stay alive at least until the shutdown starts on the main
  // thread. This assumes that |StartWithNSSDB| and other methods directly
  // using |database_| are not called during shutdown.
  void StartWithNSSDB(net::NSSCertDatabase* database);

  void AddObserver(CertLoader::Observer* observer);
  void RemoveObserver(CertLoader::Observer* observer);

  // Returns true if |cert| is hardware backed. See also
  // ForceHardwareBackedForTesting().
  static bool IsCertificateHardwareBacked(const net::X509Certificate* cert);

  // Returns true when the certificate list has been requested but not loaded.
  bool CertificatesLoading() const;

  bool certificates_loaded() const { return certificates_loaded_; }

  // This will be empty until certificates_loaded() is true.
  const net::CertificateList& cert_list() const { return *cert_list_; }

  // Called in tests if |IsCertificateHardwareBacked()| should always return
  // true.
  static void ForceHardwareBackedForTesting();

 private:
  CertLoader();
  ~CertLoader() override;

  // Trigger a certificate load. If a certificate loading task is already in
  // progress, will start a reload once the current task is finished.
  void LoadCertificates();

  // Called if a certificate load task is finished.
  void UpdateCertificates(std::unique_ptr<net::CertificateList> cert_list);

  void NotifyCertificatesLoaded(bool initial_load);

  // net::CertDatabase::Observer
  void OnCertDBChanged() override;

  base::ObserverList<Observer> observers_;

  // Flags describing current CertLoader state.
  bool certificates_loaded_;
  bool certificates_update_required_;
  bool certificates_update_running_;

  // The user-specific NSS certificate database from which the certificates
  // should be loaded.
  net::NSSCertDatabase* database_;

  // Cached Certificates loaded from the database.
  std::unique_ptr<net::CertificateList> cert_list_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<CertLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CertLoader);
};

}  // namespace chromeos

#endif  // CHROMEOS_CERT_LOADER_H_
