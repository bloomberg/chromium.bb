// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webcontentdecryptionmodule_impl.h"

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/renderer/media/cdm_session_adapter.h"
#include "content/renderer/media/webcontentdecryptionmodulesession_impl.h"
#include "media/base/media_keys.h"

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
  if (!adapter->Initialize(UTF16ToASCII(key_system)))
    return NULL;

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
