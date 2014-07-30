// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_CERT_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_CERT_LIBRARY_H_

#include <string>

#include "base/strings/string16.h"
#include "chromeos/cert_loader.h"
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
  base::string16 GetCertDisplayStringAt(CertType type, int index) const;
  std::string GetServerCACertPEMAt(int index) const;
  std::string GetUserCertPkcs11IdAt(int index, int* slot_id) const;
  bool IsCertHardwareBackedAt(CertType type, int index) const;

  // Returns the index of a Certificate matching |pem_encoded| or -1 if none
  // found. This function may be slow depending on the number of stored
  // certificates.
  // TOOD(pneubeck): Either make this more efficient, asynchronous or get rid of
  // it.
  int GetServerCACertIndexByPEM(const std::string& pem_encoded) const;
  // Same as above but for a PKCS#11 id.
  int GetUserCertIndexByPkcs11Id(const std::string& pkcs11_id) const;

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

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_CERT_LIBRARY_H_
