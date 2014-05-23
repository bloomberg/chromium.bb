// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

class Profile;

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace chromeos {

namespace platform_keys {

// If the generation was successful, |public_key_spki_der| will contain the DER
// encoding of the SubjectPublicKeyInfo of the generated key and |error_message|
// will be empty. If it failed, |public_key_spki_der| will be empty and
// |error_message| contain an error message.
typedef base::Callback<void(const std::string& public_key_spki_der,
                            const std::string& error_message)>
    GenerateKeyCallback;

// Generates a RSA key with |modulus_length|. |token_id| is currently ignored,
// instead the user token associated with |profile| is always used. |callback|
// will be invoked with the resulting public key or an error.
void GenerateRSAKey(const std::string& token_id,
                    unsigned int modulus_length,
                    const GenerateKeyCallback& callback,
                    Profile* profile);

// If signing was successful, |signature| will be contain the signature and
// |error_message| will be empty. If it failed, |signature| will be empty and
// |error_message| contain an error message.
typedef base::Callback<void(const std::string& signature,
                            const std::string& error_message)> SignCallback;

// Signs |data| with the private key matching |public_key|, if that key is
// stored in the given token. |token_id| is currently ignored, instead the user
// token associated with |profile| is always used. |public_key| must be the DER
// encoding of a SubjectPublicKeyInfo. |callback| will be invoked with the
// signature or an error message.
// Currently supports RSA keys only.
void Sign(const std::string& token_id,
          const std::string& public_key,
          const std::string& data,
          const SignCallback& callback,
          Profile* profile);

// If the list of certificates could be successfully retrieved, |certs| will
// contain the list of available certificates (maybe empty) and |error_message|
// will be empty. If an error occurred, |certs| will be empty and
// |error_message| contain an error message.
typedef base::Callback<void(scoped_ptr<net::CertificateList> certs,
                            const std::string& error_message)>
    GetCertificatesCallback;

// Returns the list of all certificates with stored private key available from
// the given token. |token_id| is currently ignored, instead the user token
// associated with |profile| is always used. |callback| will be invoked with
// the list of available certificates or an error message.
void GetCertificates(const std::string& token_id,
                     const GetCertificatesCallback& callback,
                     Profile* profile);

// If an error occurred during import, |error_message| will be set to an error
// message.
typedef base::Callback<void(const std::string& error_message)>
    ImportCertificateCallback;

// Imports |certificate| to the given token if the certified key is already
// stored in this token. Any intermediate of |certificate| will be ignored.
// |token_id| is currently ignored, instead the user token associated with
// |profile| is always used. |callback| will be invoked when the import is
// finished, possibly with an error message.
void ImportCertificate(const std::string& token_id,
                       scoped_refptr<net::X509Certificate> certificate,
                       const ImportCertificateCallback& callback,
                       Profile* profile);

// If an error occurred during removal, |error_message| will be set to an error
// message.
typedef base::Callback<void(const std::string& error_message)>
    RemoveCertificateCallback;

// Removes |certificate| from the given token if present. Any intermediate of
// |certificate| will be ignored. |token_id| is currently ignored, instead the
// user token associated with |profile| is always used. |callback| will be
// invoked when the removal is finished, possibly with an error message.
void RemoveCertificate(const std::string& token_id,
                       scoped_refptr<net::X509Certificate> certificate,
                       const RemoveCertificateCallback& callback,
                       Profile* profile);

}  // namespace platform_keys

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_H_
