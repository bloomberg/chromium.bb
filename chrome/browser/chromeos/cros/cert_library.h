// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CERT_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CERT_LIBRARY_H_

#include <string>

#include "base/string16.h"
#include "chromeos/network/cert_loader.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {

class CertNameComparator;

// This class is responsible for keeping track of certificates in a UI
// friendly manner. It observes CertLoader to receive certificate list
// updates and sorts them by type for the UI. All public APIs are expected
// to be called from the UI thread and are non blocking. Observers will also
// be called on the UI thread.
class CertLibrary : public CertLoader::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called for any Observers whenever the certificates are loaded.
    // |initial_load| is true the first time this is called.
    virtual void OnCertificatesLoaded(bool initial_load) = 0;

   protected:
    Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  enum CertType {
    CERT_TYPE_DEFAULT,
    CERT_TYPE_USER,
    CERT_TYPE_SERVER,
    CERT_TYPE_SERVER_CA
  };

  // Manage the global instance.
  static void Initialize();
  static void Shutdown();
  static CertLibrary* Get();
  static bool IsInitialized();

  // Add / Remove Observer
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true when the certificate list has been requested but not loaded.
  bool CertificatesLoading() const;

  // Returns true when the certificate list has been initiailized.
  bool CertificatesLoaded() const;

  // Returns true if the TPM is available for hardware-backed certificates.
  bool IsHardwareBacked() const;

  // Retruns the number of certificates available for |type|.
  int NumCertificates(CertType type) const;

  // Retreives the certificate property for |type| at |index|.
  string16 GetCertDisplayStringAt(CertType type, int index) const;
  std::string GetCertNicknameAt(CertType type, int index) const;
  std::string GetCertPkcs11IdAt(CertType type, int index) const;
  bool IsCertHardwareBackedAt(CertType type, int index) const;

  // Returns the index of a Certificate matching |nickname| or -1 if none found.
  int GetCertIndexByNickname(CertType type, const std::string& nickname) const;
  // Same as above but for a PKCS#11 id. TODO(stevenjb): Replace this with a
  // better mechanism for uniquely idientifying certificates, crbug.com/236978.
  int GetCertIndexByPkcs11Id(CertType type, const std::string& pkcs11_id) const;

  // CertLoader::Observer
  virtual void OnCertificatesLoaded(const net::CertificateList&,
                                    bool initial_load) OVERRIDE;

 private:
  CertLibrary();
  virtual ~CertLibrary();

  net::X509Certificate* GetCertificateAt(CertType type, int index) const;
  const net::CertificateList& GetCertificateListForType(CertType type) const;

  ObserverList<CertLibrary::Observer> observer_list_;

  // Sorted certificate lists
  net::CertificateList certs_;
  net::CertificateList user_certs_;
  net::CertificateList server_certs_;
  net::CertificateList server_ca_certs_;

  DISALLOW_COPY_AND_ASSIGN(CertLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CERT_LIBRARY_H_
