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
#include "net/ssl/ssl_client_cert_type.h"

namespace content {
class BrowserContext;
}

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace chromeos {

namespace platform_keys {

// A token is a store for keys or certs and can provide cryptographic
// operations.
// ChromeOS provides itself a user token and conditionally a system wide token,
// thus these tokens use static identifiers. The platform keys API is designed
// to support arbitrary other tokens in the future, which could then use
// run-time generated IDs.
extern const char kTokenIdUser[];
extern const char kTokenIdSystem[];

// Supported hash algorithms.
enum HashAlgorithm {
  HASH_ALGORITHM_SHA1,
  HASH_ALGORITHM_SHA256,
  HASH_ALGORITHM_SHA384,
  HASH_ALGORITHM_SHA512
};

struct ClientCertificateRequest {
  ClientCertificateRequest();
  ~ClientCertificateRequest();

  // The list of the types of certificates requested, sorted in order of the
  // server's preference.
  std::vector<net::SSLClientCertType> certificate_key_types;

  // List of distinguished names of certificate authorities allowed by the
  // server. Each entry must be a DER-encoded X.509 DistinguishedName.
  std::vector<std::string> certificate_authorities;
};

namespace subtle {
// Functions of this namespace shouldn't be called directly from the context of
// an extension. Instead use PlatformKeysService which enforces restrictions
// upon extensions.

typedef base::Callback<void(const std::string& public_key_spki_der,
                            const std::string& error_message)>
    GenerateKeyCallback;

// Generates a RSA key pair with |modulus_length_bits|. |token_id| is currently
// ignored, instead the user token associated with |browser_context| is always
// used. |callback| will be invoked with the resulting public key or an error.
void GenerateRSAKey(const std::string& token_id,
                    unsigned int modulus_length_bits,
                    const GenerateKeyCallback& callback,
                    content::BrowserContext* browser_context);

typedef base::Callback<void(const std::string& signature,
                            const std::string& error_message)> SignCallback;

// Digests |data| with |hash_algorithm| and afterwards signs the digest with the
// private key matching |public_key|, if that key is stored in the given token.
// |token_id| is currently ignored, instead the user token associated with
// |browser_context| is always used. |public_key| must be the DER encoding of a
// SubjectPublicKeyInfo. |callback| will be invoked with the signature or an
// error message.
// Currently supports RSA keys only.
void Sign(const std::string& token_id,
          const std::string& public_key,
          HashAlgorithm hash_algorithm,
          const std::string& data,
          const SignCallback& callback,
          content::BrowserContext* browser_context);

// If the certificate request could be processed successfully, |matches| will
// contain the list of matching certificates (which may be empty) and
// |error_message| will be empty. If an error occurred, |matches| will be null
// and |error_message| contain an error message.
typedef base::Callback<void(scoped_ptr<net::CertificateList> matches,
                            const std::string& error_message)>
    SelectCertificatesCallback;

// Returns the list of all certificates that match |request|. |callback| will be
// invoked with these matches or an error message.
void SelectClientCertificates(const ClientCertificateRequest& request,
                              const SelectCertificatesCallback& callback,
                              content::BrowserContext* browser_context);

}  // namespace subtle

// If the list of certificates could be successfully retrieved, |certs| will
// contain the list of available certificates (maybe empty) and |error_message|
// will be empty. If an error occurred, |certs| will be empty and
// |error_message| contain an error message.
typedef base::Callback<void(scoped_ptr<net::CertificateList> certs,
                            const std::string& error_message)>
    GetCertificatesCallback;

// Returns the list of all certificates with stored private key available from
// the given token. |token_id| is currently ignored, instead the user token
// associated with |browser_context| is always used. |callback| will be invoked
// with the list of available certificates or an error message.
void GetCertificates(const std::string& token_id,
                     const GetCertificatesCallback& callback,
                     content::BrowserContext* browser_context);

// If an error occurred during import, |error_message| will be set to an error
// message.
typedef base::Callback<void(const std::string& error_message)>
    ImportCertificateCallback;

// Imports |certificate| to the given token if the certified key is already
// stored in this token. Any intermediate of |certificate| will be ignored.
// |token_id| is currently ignored, instead the user token associated with
// |browser_context| is always used. |callback| will be invoked when the import
// is finished, possibly with an error message.
void ImportCertificate(const std::string& token_id,
                       scoped_refptr<net::X509Certificate> certificate,
                       const ImportCertificateCallback& callback,
                       content::BrowserContext* browser_context);

// If an error occurred during removal, |error_message| will be set to an error
// message.
typedef base::Callback<void(const std::string& error_message)>
    RemoveCertificateCallback;

// Removes |certificate| from the given token if present. Any intermediate of
// |certificate| will be ignored. |token_id| is currently ignored, instead the
// user token associated with |browser_context| is always used. |callback| will
// be invoked when the removal is finished, possibly with an error message.
void RemoveCertificate(const std::string& token_id,
                       scoped_refptr<net::X509Certificate> certificate,
                       const RemoveCertificateCallback& callback,
                       content::BrowserContext* browser_context);

// If the list of available tokens could be successfully retrieved, |token_ids|
// will contain the token ids. If an error occurs, |token_ids| will be NULL and
// |error_message| will be set to an error message.
typedef base::Callback<void(scoped_ptr<std::vector<std::string> > token_ids,
                            const std::string& error_message)>
    GetTokensCallback;

// Gets the list of available tokens. |callback| will be invoked when the list
// of available tokens is determined, possibly with an error message.
// Must be called and calls |callback| on the UI thread.
void GetTokens(const GetTokensCallback& callback,
               content::BrowserContext* browser_context);

}  // namespace platform_keys

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_H_
