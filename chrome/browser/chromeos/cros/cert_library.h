// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CERT_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CERT_LIBRARY_H_

#include <string>

#include "base/string16.h"
#include "net/base/x509_certificate.h"

namespace crypto {
class SymmetricKey;
}

namespace chromeos {

class CertLibrary {
 public:

  // Observers can register themselves via CertLibrary::AddObserver, and can
  // un-register with CertLibrary::RemoveObserver.
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

  // Wrapper class to provide an additional interface for net::CertificateList.
  class CertList {
   public:
    explicit CertList(CertLibrary* library);
    ~CertList();
    void Append(net::X509Certificate* cert) { list_.push_back(cert); }
    void Clear() { list_.clear(); }
    int Size() const { return static_cast<int>(list_.size()); }
    net::X509Certificate* GetCertificateAt(int index) const;
    string16 GetDisplayStringAt(int index) const;  // User-visible name.
    std::string GetNicknameAt(int index) const;
    std::string GetPkcs11IdAt(int index) const;
    bool IsHardwareBackedAt(int index) const;
    // Finds the index of a Certificate matching |nickname|.
    // Returns -1 if none found.
    int FindCertByNickname(const std::string& nickname) const;
    // Same as above but for a pkcs#11 id.
    int FindCertByPkcs11Id(const std::string& pkcs11_id) const;
    net::CertificateList& list() { return list_; }
   private:
    net::CertificateList list_;
    CertLibrary* cert_library_;

    DISALLOW_COPY_AND_ASSIGN(CertList);
  };

  virtual ~CertLibrary();

  static CertLibrary* GetImpl(bool stub);

  // Registers |observer|. The thread on which this is called is the thread
  // on which |observer| will be called back with notifications.
  virtual void AddObserver(Observer* observer) = 0;

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddObserver() was called.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Loads the key/certificates database for the current logged in user.
  virtual void LoadKeyStore() = 0;

  // Returns true when the certificate list has been requested but not loaded.
  virtual bool CertificatesLoading() const = 0;

  // Returns true when the certificate list has been initiailized.
  virtual bool CertificatesLoaded() const = 0;

  // Returns true if the TPM is available for hardware-backed certificates.
  virtual bool IsHardwareBacked() const = 0;

  // Returns the cached TPM token name.
  virtual const std::string& GetTpmTokenName() const = 0;

  // Returns the current list of all certificates.
  virtual const CertList& GetCertificates() const = 0;

  // Returns the current list of user certificates.
  virtual const CertList& GetUserCertificates() const = 0;

  // Returns the current list of server certificates.
  virtual const CertList& GetServerCertificates() const = 0;

  // Returns the current list of server CA certificates.
  virtual const CertList& GetCACertificates() const = 0;

  // Encrypts |token| with supplemental user key.
  virtual std::string EncryptToken(const std::string& token) = 0;

  // Decrypts |token| with supplemental user key.
  virtual std::string DecryptToken(const std::string& encrypted_token) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CERT_LIBRARY_H_
