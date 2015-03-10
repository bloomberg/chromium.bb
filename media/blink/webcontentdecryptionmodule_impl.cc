// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webcontentdecryptionmodule_impl.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/cdm_promise.h"
#include "media/base/key_systems.h"
#include "media/base/media_keys.h"
#include "media/blink/cdm_result_promise.h"
#include "media/blink/cdm_session_adapter.h"
#include "media/blink/webcontentdecryptionmodulesession_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "url/gurl.h"

namespace media {

void WebContentDecryptionModuleImpl::Create(
    media::CdmFactory* cdm_factory,
    const base::string16& key_system,
    bool allow_distinctive_identifier,
    bool allow_persistent_state,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(!security_origin.isNull());
  DCHECK(!key_system.empty());

  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII key systems.
  if (!base::IsStringASCII(key_system)) {
    NOTREACHED();
    result.completeWithError(
        blink::WebContentDecryptionModuleExceptionNotSupportedError, 0,
        "Invalid keysystem.");
    return;
  }

  // TODO(ddorwin): This should be a DCHECK.
  std::string key_system_ascii = base::UTF16ToASCII(key_system);
  if (!media::IsSupportedKeySystem(key_system_ascii)) {
    std::string message =
        "Keysystem '" + key_system_ascii + "' is not supported.";
    result.completeWithError(
        blink::WebContentDecryptionModuleExceptionNotSupportedError, 0,
        blink::WebString::fromUTF8(message));
    return;
  }

  // If unique security origin, don't try to create the CDM.
  if (security_origin.isUnique() || security_origin.toString() == "null") {
    result.completeWithError(
        blink::WebContentDecryptionModuleExceptionNotSupportedError, 0,
        "CDM use not allowed for unique security origin.");
    return;
  }

  GURL security_origin_as_gurl(security_origin.toString());
  scoped_refptr<CdmSessionAdapter> adapter(new CdmSessionAdapter());

  // TODO(jrummell): Pass WebContentDecryptionModuleResult (or similar) to
  // Initialize() so that more specific errors can be reported.
  if (!adapter->Initialize(cdm_factory, key_system_ascii,
                           allow_distinctive_identifier,
                           allow_persistent_state, security_origin_as_gurl)) {
    result.completeWithError(
        blink::WebContentDecryptionModuleExceptionNotSupportedError, 0,
        "Failed to initialize CDM.");
    return;
  }

  result.completeWithContentDecryptionModule(
      new WebContentDecryptionModuleImpl(adapter));
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

void WebContentDecryptionModuleImpl::setServerCertificate(
    const uint8* server_certificate,
    size_t server_certificate_length,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(server_certificate);
  adapter_->SetServerCertificate(
      server_certificate,
      base::saturated_cast<int>(server_certificate_length),
      scoped_ptr<SimpleCdmPromise>(
          new CdmResultPromise<>(result, std::string())));
}

CdmContext* WebContentDecryptionModuleImpl::GetCdmContext() {
  return adapter_->GetCdmContext();
}

}  // namespace media
