// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace base {
class ListValue;
class Value;
}

namespace extensions {
class StateStore;
}

namespace chromeos {

class PlatformKeysService : public KeyedService {
 public:
  // Stores registration information in |state_store|, i.e. for each extension
  // the list of public keys that are valid to be used for signing. Each key can
  // be used for signing at most once.
  // The format written to |state_store| is:
  //   kStateStorePlatformKeys maps to a list of strings.
  //   Each string is the base64 encoding of the DER representation of a public
  //   key's SPKI.
  explicit PlatformKeysService(content::BrowserContext* browser_context,
                               extensions::StateStore* state_store);
  virtual ~PlatformKeysService();

  // If the generation was successful, |public_key_spki_der| will contain the
  // DER encoding of the SubjectPublicKeyInfo of the generated key and
  // |error_message| will be empty. If it failed, |public_key_spki_der| will be
  // empty and |error_message| contain an error message.
  typedef base::Callback<void(const std::string& public_key_spki_der,
                              const std::string& error_message)>
      GenerateKeyCallback;

  // Generates a RSA key pair with |modulus_length_bits| and registers the key
  // to allow a single sign operation by the given extension. |token_id| is
  // currently ignored, instead the user token associated with |browser_context|
  // is always used. |callback| will be invoked with the resulting public key or
  // an error.
  // Will only call back during the lifetime of this object.
  void GenerateRSAKey(const std::string& token_id,
                      unsigned int modulus_length_bits,
                      const std::string& extension_id,
                      const GenerateKeyCallback& callback);

  // If signing was successful, |signature| will be contain the signature and
  // |error_message| will be empty. If it failed, |signature| will be empty and
  // |error_message| contain an error message.
  typedef base::Callback<void(const std::string& signature,
                              const std::string& error_message)> SignCallback;

  // Digests |data| with |hash_algorithm| and afterwards signs the digest with
  // the private key matching |public_key_spki_der|, if that key is stored in
  // the given token and wasn't used for signing before.
  // Unregisters the key so that every future attempt to sign data with this key
  // is rejected. |token_id| is currently ignored, instead the user token
  // associated with |browser_context| is always used. |public_key_spki_der|
  // must be the DER encoding of a SubjectPublicKeyInfo. |callback| will be
  // invoked with the signature or an error message. Currently supports RSA keys
  // only.
  // Will only call back during the lifetime of this object.
  void Sign(const std::string& token_id,
            const std::string& public_key_spki_der,
            platform_keys::HashAlgorithm hash_algorithm,
            const std::string& data,
            const std::string& extension_id,
            const SignCallback& callback);

 private:
  typedef base::Callback<void(scoped_ptr<base::ListValue> platform_keys)>
      GetPlatformKeysCallback;

  // Registers the given public key as newly generated key, which is allowed to
  // be used for signing for a single time. Afterwards, calls |callback|. If
  // registration was successful, passes |true| otherwise |false| to the
  // callback.
  void RegisterPublicKey(const std::string& extension_id,
                         const std::string& public_key_spki_der,
                         const base::Callback<void(bool)>& callback);

  // Gets the current validity of the given public key by reading StateStore.
  // Invalidates the key if it was found to be valid. Finally, calls |callback|
  // with the old validity.
  void ReadValidityAndInvalidateKey(const std::string& extension_id,
                                    const std::string& public_key_spki_der,
                                    const base::Callback<void(bool)>& callback);

  // Reads the list of public keys currently registered for |extension_id| from
  // StateStore. Calls |callback| with the read list, or a new empty list if
  // none existed. If an error occurred, calls |callback| with NULL.
  void GetPlatformKeysOfExtension(const std::string& extension_id,
                                  const GetPlatformKeysCallback& callback);

  // Callback used by |GenerateRSAKey|.
  // If the key generation was successful, registers the generated public key
  // for the given extension. If any error occurs during key generation or
  // registration, calls |callback| with an error. Otherwise, on success, calls
  // |callback| with the public key.
  void GenerateRSAKeyCallback(const std::string& extension_id,
                              const GenerateKeyCallback& callback,
                              const std::string& public_key_spki_der,
                              const std::string& error_message);

  // Callback used by |RegisterPublicKey|.
  // Updates the old |platform_keys| read from the StateStore and writes the
  // updated value back to the StateStore.
  void RegisterPublicKeyGotPlatformKeys(
      const std::string& extension_id,
      const std::string& public_key_spki_der,
      const base::Callback<void(bool)>& callback,
      scoped_ptr<base::ListValue> platform_keys);

  // Callback used by |ReadValidityAndInvalidateKey|.
  // Invalidates the given public key so that future signing is prohibited and
  // calls |callback| with the old validity.
  void InvalidateKey(const std::string& extension_id,
                     const std::string& public_key_spki_der,
                     const base::Callback<void(bool)>& callback,
                     scoped_ptr<base::ListValue> platform_keys);

  // Callback used by |GetPlatformKeysOfExtension|.
  // Is called with |value| set to the PlatformKeys value read from the
  // StateStore, which it forwards to |callback|. On error, calls |callback|
  // with NULL; if no value existed, with an empty list.
  void GotPlatformKeysOfExtension(const std::string& extension_id,
                                  const GetPlatformKeysCallback& callback,
                                  scoped_ptr<base::Value> value);

  content::BrowserContext* browser_context_;
  extensions::StateStore* state_store_;
  base::WeakPtrFactory<PlatformKeysService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformKeysService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_H_
