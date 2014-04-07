// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CERT_LOADER_H_
#define CHROMEOS_CERT_LOADER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
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
  // hex string, or the empty string if none is found. Note that the returned ID
  // should be used only to identify the cert in its slot.
  // This should be used only for user certificates, assuming that only one
  // private slot is loaded for a user.
  // TODO(tbarzic): Make this check cert slot id if we start loading
  // certificates for secondary users.
  static std::string GetPkcs11IdForCert(const net::X509Certificate& cert);

  // Starts the CertLoader with the NSS cert database.
  // The CertLoader will _not_ take the ownership of the database, but it
  // expects it to stay alive at least until the shutdown starts on the main
  // thread. This assumes that |StartWithNSSDB| and other methods directly
  // using |database_| are not called during shutdown.
  void StartWithNSSDB(net::NSSCertDatabase* database);

  void AddObserver(CertLoader::Observer* observer);
  void RemoveObserver(CertLoader::Observer* observer);

  int TPMTokenSlotID() const;
  bool IsHardwareBacked() const;

  // Whether the certificate is hardware backed. Returns false if the CertLoader
  // was not yet started (both |CertificatesLoading()| and
  // |certificates_loaded()| are false).
  bool IsCertificateHardwareBacked(const net::X509Certificate* cert) const;

  // Returns true when the certificate list has been requested but not loaded.
  bool CertificatesLoading() const;

  bool certificates_loaded() const { return certificates_loaded_; }

  // This will be empty until certificates_loaded() is true.
  const net::CertificateList& cert_list() const { return *cert_list_; }

  void force_hardware_backed_for_test() {
    force_hardware_backed_for_test_ = true;
  }

 private:
  CertLoader();
  virtual ~CertLoader();

  // Trigger a certificate load. If a certificate loading task is already in
  // progress, will start a reload once the current task is finished.
  void LoadCertificates();

  // Called if a certificate load task is finished.
  void UpdateCertificates(scoped_ptr<net::CertificateList> cert_list);

  void NotifyCertificatesLoaded(bool initial_load);

  // net::CertDatabase::Observer
  virtual void OnCACertChanged(const net::X509Certificate* cert) OVERRIDE;
  virtual void OnCertAdded(const net::X509Certificate* cert) OVERRIDE;
  virtual void OnCertRemoved(const net::X509Certificate* cert) OVERRIDE;

  ObserverList<Observer> observers_;

  // Flags describing current CertLoader state.
  bool certificates_loaded_;
  bool certificates_update_required_;
  bool certificates_update_running_;

  // The user-specific NSS certificate database from which the certificates
  // should be loaded.
  net::NSSCertDatabase* database_;

  // Set during tests if |IsHardwareBacked()| should always return true.
  bool force_hardware_backed_for_test_;

  // Cached Certificates loaded from the database.
  scoped_ptr<net::CertificateList> cert_list_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<CertLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CertLoader);
};

}  // namespace chromeos

#endif  // CHROMEOS_CERT_LOADER_H_
