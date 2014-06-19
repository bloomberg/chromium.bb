// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/common/extensions/api/enterprise_platform_keys.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_internal.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"

namespace extensions {

namespace {

namespace api_epk = api::enterprise_platform_keys;
namespace api_epki = api::enterprise_platform_keys_internal;

// This error will occur if a token is removed and will be exposed to the
// extension. Keep this in sync with the custom binding in Javascript.
const char kErrorInvalidToken[] = "The token is not valid.";

const char kErrorAlgorithmNotSupported[] = "Algorithm not supported.";
const char kErrorInvalidX509Cert[] =
    "Certificate is not a valid X.509 certificate.";
const char kTokenIdUser[] = "user";

// Returns whether |token_id| references a known Token.
bool ValidateToken(const std::string& token_id) {
  // For now, the user token is the only valid one.
  return token_id == kTokenIdUser;
}

}  // namespace

EnterprisePlatformKeysInternalGenerateKeyFunction::
    ~EnterprisePlatformKeysInternalGenerateKeyFunction() {
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalGenerateKeyFunction::Run() {
  scoped_ptr<api_epki::GenerateKey::Params> params(
      api_epki::GenerateKey::Params::Create(*args_));
  // TODO(pneubeck): Add support for unsigned integers to IDL.
  EXTENSION_FUNCTION_VALIDATE(params && params->modulus_length >= 0);
  if (!ValidateToken(params->token_id))
    return RespondNow(Error(kErrorInvalidToken));

  chromeos::PlatformKeysService* service =
      chromeos::PlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  service->GenerateRSAKey(
      params->token_id,
      params->modulus_length,
      extension_id(),
      base::Bind(
          &EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey,
          this));
  return RespondLater();
}

void EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey(
    const std::string& public_key_der,
    const std::string& error_message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (error_message.empty()) {
    Respond(
        ArgumentList(api_epki::GenerateKey::Results::Create(public_key_der)));
  } else {
    Respond(Error(error_message));
  }
}

EnterprisePlatformKeysInternalSignFunction::
    ~EnterprisePlatformKeysInternalSignFunction() {
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalSignFunction::Run() {
  scoped_ptr<api_epki::Sign::Params> params(
      api_epki::Sign::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!ValidateToken(params->token_id))
    return RespondNow(Error(kErrorInvalidToken));

  chromeos::platform_keys::HashAlgorithm hash_algorithm;
  if (params->hash_algorithm_name == "SHA-1")
    hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA1;
  else if (params->hash_algorithm_name == "SHA-256")
    hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA256;
  else if (params->hash_algorithm_name == "SHA-384")
    hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA384;
  else if (params->hash_algorithm_name == "SHA-512")
    hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA512;
  else
    return RespondNow(Error(kErrorAlgorithmNotSupported));

  chromeos::PlatformKeysService* service =
      chromeos::PlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  service->Sign(
      params->token_id,
      params->public_key,
      hash_algorithm,
      params->data,
      extension_id(),
      base::Bind(&EnterprisePlatformKeysInternalSignFunction::OnSigned, this));
  return RespondLater();
}

void EnterprisePlatformKeysInternalSignFunction::OnSigned(
    const std::string& signature,
    const std::string& error_message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (error_message.empty())
    Respond(ArgumentList(api_epki::Sign::Results::Create(signature)));
  else
    Respond(Error(error_message));
}

EnterprisePlatformKeysGetCertificatesFunction::
    ~EnterprisePlatformKeysGetCertificatesFunction() {
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysGetCertificatesFunction::Run() {
  scoped_ptr<api_epk::GetCertificates::Params> params(
      api_epk::GetCertificates::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!ValidateToken(params->token_id))
    return RespondNow(Error(kErrorInvalidToken));

  chromeos::platform_keys::GetCertificates(
      params->token_id,
      base::Bind(
          &EnterprisePlatformKeysGetCertificatesFunction::OnGotCertificates,
          this),
      browser_context());
  return RespondLater();
}

void EnterprisePlatformKeysGetCertificatesFunction::OnGotCertificates(
    scoped_ptr<net::CertificateList> certs,
    const std::string& error_message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!error_message.empty()) {
    Respond(Error(error_message));
    return;
  }

  scoped_ptr<base::ListValue> client_certs(new base::ListValue());
  for (net::CertificateList::const_iterator it = certs->begin();
       it != certs->end();
       ++it) {
    std::string der_encoding;
    net::X509Certificate::GetDEREncoded((*it)->os_cert_handle(), &der_encoding);
    client_certs->Append(base::BinaryValue::CreateWithCopiedBuffer(
        der_encoding.data(), der_encoding.size()));
  }

  scoped_ptr<base::ListValue> results(new base::ListValue());
  results->Append(client_certs.release());
  Respond(ArgumentList(results.Pass()));
}

EnterprisePlatformKeysImportCertificateFunction::
    ~EnterprisePlatformKeysImportCertificateFunction() {
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysImportCertificateFunction::Run() {
  scoped_ptr<api_epk::ImportCertificate::Params> params(
      api_epk::ImportCertificate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!ValidateToken(params->token_id))
    return RespondNow(Error(kErrorInvalidToken));

  const std::string& cert_der = params->certificate;
  scoped_refptr<net::X509Certificate> cert_x509 =
      net::X509Certificate::CreateFromBytes(cert_der.data(), cert_der.size());
  if (!cert_x509)
    return RespondNow(Error(kErrorInvalidX509Cert));

  chromeos::platform_keys::ImportCertificate(
      params->token_id,
      cert_x509,
      base::Bind(&EnterprisePlatformKeysImportCertificateFunction::
                     OnImportedCertificate,
                 this),
      browser_context());
  return RespondLater();
}

void EnterprisePlatformKeysImportCertificateFunction::OnImportedCertificate(
    const std::string& error_message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (error_message.empty())
    Respond(NoArguments());
  else
    Respond(Error(error_message));
}

EnterprisePlatformKeysRemoveCertificateFunction::
    ~EnterprisePlatformKeysRemoveCertificateFunction() {
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysRemoveCertificateFunction::Run() {
  scoped_ptr<api_epk::RemoveCertificate::Params> params(
      api_epk::RemoveCertificate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!ValidateToken(params->token_id))
    return RespondNow(Error(kErrorInvalidToken));

  const std::string& cert_der = params->certificate;
  scoped_refptr<net::X509Certificate> cert_x509 =
      net::X509Certificate::CreateFromBytes(cert_der.data(), cert_der.size());
  if (!cert_x509)
    return RespondNow(Error(kErrorInvalidX509Cert));

  chromeos::platform_keys::RemoveCertificate(
      params->token_id,
      cert_x509,
      base::Bind(&EnterprisePlatformKeysRemoveCertificateFunction::
                     OnRemovedCertificate,
                 this),
      browser_context());
  return RespondLater();
}

void EnterprisePlatformKeysRemoveCertificateFunction::OnRemovedCertificate(
    const std::string& error_message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (error_message.empty())
    Respond(NoArguments());
  else
    Respond(Error(error_message));
}

EnterprisePlatformKeysInternalGetTokensFunction::
    ~EnterprisePlatformKeysInternalGetTokensFunction() {
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalGetTokensFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args_->empty());

  std::vector<std::string> token_ids;
  token_ids.push_back(kTokenIdUser);
  return RespondNow(
      ArgumentList(api_epki::GetTokens::Results::Create(token_ids)));
}

}  // namespace extensions
