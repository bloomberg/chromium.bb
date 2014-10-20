// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webcontentdecryptionmodule_impl.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/cdm_session_adapter.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "content/renderer/media/webcontentdecryptionmodulesession_impl.h"
#include "media/base/cdm_promise.h"
#include "media/base/media_keys.h"
#include "media/blink/cdm_result_promise.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "url/gurl.h"

namespace content {

WebContentDecryptionModuleImpl* WebContentDecryptionModuleImpl::Create(
    media::CdmFactory* cdm_factory,
    const blink::WebSecurityOrigin& security_origin,
    const base::string16& key_system) {
  DCHECK(!security_origin.isNull());
  DCHECK(!key_system.empty());

  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII key systems.
  if (!base::IsStringASCII(key_system)) {
    NOTREACHED();
    return NULL;
  }

  std::string key_system_ascii = base::UTF16ToASCII(key_system);
  if (!IsConcreteSupportedKeySystem(key_system_ascii))
    return NULL;

  // If unique security origin, don't try to create the CDM.
  if (security_origin.isUnique() || security_origin.toString() == "null") {
    DLOG(ERROR) << "CDM use not allowed for unique security origin.";
    return NULL;
  }

  scoped_refptr<CdmSessionAdapter> adapter(new CdmSessionAdapter());
  GURL security_origin_as_gurl(security_origin.toString());

  if (!adapter->Initialize(
          cdm_factory, key_system_ascii, security_origin_as_gurl)) {
    return NULL;
  }

  return new WebContentDecryptionModuleImpl(adapter);
}

WebContentDecryptionModuleImpl::WebContentDecryptionModuleImpl(
    scoped_refptr<CdmSessionAdapter> adapter)
    : adapter_(adapter) {
}

WebContentDecryptionModuleImpl::~WebContentDecryptionModuleImpl() {
}

// The caller owns the created session.
blink::WebContentDecryptionModuleSession*
WebContentDecryptionModuleImpl::createSession() {
  return adapter_->CreateSession();
}

blink::WebContentDecryptionModuleSession*
WebContentDecryptionModuleImpl::createSession(
    blink::WebContentDecryptionModuleSession::Client* client) {
  WebContentDecryptionModuleSessionImpl* session = adapter_->CreateSession();
  session->setClientInterface(client);
  return session;
}

void WebContentDecryptionModuleImpl::setServerCertificate(
    const uint8* server_certificate,
    size_t server_certificate_length,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(server_certificate);
  adapter_->SetServerCertificate(
      server_certificate,
      server_certificate_length,
      scoped_ptr<media::SimpleCdmPromise>(
          new media::CdmResultPromise<>(result, std::string())));
}

media::Decryptor* WebContentDecryptionModuleImpl::GetDecryptor() {
  return adapter_->GetDecryptor();
}

#if defined(ENABLE_BROWSER_CDMS)
int WebContentDecryptionModuleImpl::GetCdmId() const {
  return adapter_->GetCdmId();
}
#endif  // defined(ENABLE_BROWSER_CDMS)

}  // namespace content
