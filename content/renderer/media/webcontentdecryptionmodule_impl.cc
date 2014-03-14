// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webcontentdecryptionmodule_impl.h"

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/cdm_session_adapter.h"
#include "content/renderer/media/webcontentdecryptionmodulesession_impl.h"
#include "media/base/media_keys.h"

#if defined(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/crypto/pepper_cdm_wrapper_impl.h"
#endif

namespace content {

WebContentDecryptionModuleImpl* WebContentDecryptionModuleImpl::Create(
    const base::string16& key_system) {
  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII key systems.
  if (!IsStringASCII(key_system)) {
    NOTREACHED();
    return NULL;
  }

  scoped_refptr<CdmSessionAdapter> adapter(new CdmSessionAdapter());
  if (!adapter->Initialize(
#if defined(ENABLE_PEPPER_CDMS)
          // TODO(jrummell): Figure out how to get a WebFrame from Blink (or
          // something equivalent) so the plugin can actually get created.
          // http://crbug.com/250049
          base::Bind(&PepperCdmWrapperImpl::Create,
                     static_cast<blink::WebFrame*>(NULL)),
#endif
          base::UTF16ToASCII(key_system))) {
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
WebContentDecryptionModuleImpl::createSession(
    blink::WebContentDecryptionModuleSession::Client* client) {
  return adapter_->CreateSession(client);
}

media::Decryptor* WebContentDecryptionModuleImpl::GetDecryptor() {
  return adapter_->GetDecryptor();
}

}  // namespace content
