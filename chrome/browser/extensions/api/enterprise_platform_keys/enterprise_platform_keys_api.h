// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/chrome_extension_function.h"

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace extensions {

class EnterprisePlatformKeysInternalGenerateKeyFunction
    : public ChromeUIThreadExtensionFunction {
 private:
  ~EnterprisePlatformKeysInternalGenerateKeyFunction() override;
  ResponseAction Run() override;

  // Called when the key was generated. If an error occurred, |public_key_der|
  // will be empty and instead |error_message| be set.
  void OnGeneratedKey(const std::string& public_key_der,
                      const std::string& error_message);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeysInternal.generateKey",
                             ENTERPRISE_PLATFORMKEYSINTERNAL_GENERATEKEY);
};

class EnterprisePlatformKeysGetCertificatesFunction
    : public ChromeUIThreadExtensionFunction {
 private:
  ~EnterprisePlatformKeysGetCertificatesFunction() override;
  ResponseAction Run() override;

  // Called when the list of certificates was determined. If an error occurred,
  // |certs| will be NULL and instead |error_message| be set.
  void OnGotCertificates(scoped_ptr<net::CertificateList> certs,
                         const std::string& error_message);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.getCertificates",
                             ENTERPRISE_PLATFORMKEYS_GETCERTIFICATES);
};

class EnterprisePlatformKeysImportCertificateFunction
    : public ChromeUIThreadExtensionFunction {
 private:
  ~EnterprisePlatformKeysImportCertificateFunction() override;
  ResponseAction Run() override;

  // Called when the certificate was imported. Only if an error occurred,
  // |error_message| will be set.
  void OnImportedCertificate(const std::string& error_message);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.importCertificate",
                             ENTERPRISE_PLATFORMKEYS_IMPORTCERTIFICATE);
};

class EnterprisePlatformKeysRemoveCertificateFunction
    : public ChromeUIThreadExtensionFunction {
 private:
  ~EnterprisePlatformKeysRemoveCertificateFunction() override;
  ResponseAction Run() override;

  // Called when the certificate was removed. Only if an error occurred,
  // |error_message| will be set.
  void OnRemovedCertificate(const std::string& error_message);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.removeCertificate",
                             ENTERPRISE_PLATFORMKEYS_REMOVECERTIFICATE);
};

class EnterprisePlatformKeysInternalGetTokensFunction
    : public ChromeUIThreadExtensionFunction {
 private:
  ~EnterprisePlatformKeysInternalGetTokensFunction() override;
  ResponseAction Run() override;

  // Called when the list of tokens was determined. If an error occurred,
  // |token_ids| will be NULL and instead |error_message| be set.
  void OnGotTokens(scoped_ptr<std::vector<std::string> > token_ids,
                   const std::string& error_message);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeysInternal.getTokens",
                             ENTERPRISE_PLATFORMKEYSINTERNAL_GETTOKENS);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_H_
