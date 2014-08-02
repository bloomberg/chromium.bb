// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
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
  virtual ~EnterprisePlatformKeysInternalGenerateKeyFunction();
  virtual ResponseAction Run() OVERRIDE;

  // Called when the key was generated. If an error occurred, |public_key_der|
  // will be empty and instead |error_message| be set.
  void OnGeneratedKey(const std::string& public_key_der,
                      const std::string& error_message);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeysInternal.generateKey",
                             ENTERPRISE_PLATFORMKEYSINTERNAL_GENERATEKEY);
};

class EnterprisePlatformKeysInternalSignFunction
    : public ChromeUIThreadExtensionFunction {
 private:
  virtual ~EnterprisePlatformKeysInternalSignFunction();
  virtual ResponseAction Run() OVERRIDE;

  // Called when the signature was generated. If an error occurred,
  // |signature| will be empty and instead |error_message| be set.
  void OnSigned(const std::string& signature, const std::string& error_message);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeysInternal.sign",
                             ENTERPRISE_PLATFORMKEYSINTERNAL_SIGN);
};

class EnterprisePlatformKeysGetCertificatesFunction
    : public ChromeUIThreadExtensionFunction {
 private:
  virtual ~EnterprisePlatformKeysGetCertificatesFunction();
  virtual ResponseAction Run() OVERRIDE;

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
  virtual ~EnterprisePlatformKeysImportCertificateFunction();
  virtual ResponseAction Run() OVERRIDE;

  // Called when the certificate was imported. Only if an error occurred,
  // |error_message| will be set.
  void OnImportedCertificate(const std::string& error_message);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.importCertificate",
                             ENTERPRISE_PLATFORMKEYS_IMPORTCERTIFICATE);
};

class EnterprisePlatformKeysRemoveCertificateFunction
    : public ChromeUIThreadExtensionFunction {
 private:
  virtual ~EnterprisePlatformKeysRemoveCertificateFunction();
  virtual ResponseAction Run() OVERRIDE;

  // Called when the certificate was removed. Only if an error occurred,
  // |error_message| will be set.
  void OnRemovedCertificate(const std::string& error_message);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.removeCertificate",
                             ENTERPRISE_PLATFORMKEYS_REMOVECERTIFICATE);
};

class EnterprisePlatformKeysInternalGetTokensFunction
    : public ChromeUIThreadExtensionFunction {
 private:
  virtual ~EnterprisePlatformKeysInternalGetTokensFunction();
  virtual ResponseAction Run() OVERRIDE;

  // Called when the list of tokens was determined. If an error occurred,
  // |token_ids| will be NULL and instead |error_message| be set.
  void OnGotTokens(scoped_ptr<std::vector<std::string> > token_ids,
                   const std::string& error_message);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeysInternal.getTokens",
                             ENTERPRISE_PLATFORMKEYSINTERNAL_GETTOKENS);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_H_
