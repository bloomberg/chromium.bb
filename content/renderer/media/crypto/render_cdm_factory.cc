// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/render_cdm_factory.h"

#include "base/logging.h"
#include "media/base/key_systems.h"
#include "media/cdm/aes_decryptor.h"
#include "url/gurl.h"

#if defined(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/crypto/ppapi_decryptor.h"
#elif defined(ENABLE_BROWSER_CDMS)
#include "content/renderer/media/crypto/proxy_media_keys.h"
#endif  // defined(ENABLE_PEPPER_CDMS)

namespace content {

#if defined(ENABLE_PEPPER_CDMS)
RenderCdmFactory::RenderCdmFactory(
    const CreatePepperCdmCB& create_pepper_cdm_cb)
    : create_pepper_cdm_cb_(create_pepper_cdm_cb) {
}
#elif defined(ENABLE_BROWSER_CDMS)
RenderCdmFactory::RenderCdmFactory(RendererCdmManager* manager)
    : manager_(manager) {
}
#else
RenderCdmFactory::RenderCdmFactory() {
}
#endif  // defined(ENABLE_PEPPER_CDMS)

RenderCdmFactory::~RenderCdmFactory() {
}

scoped_ptr<media::MediaKeys> RenderCdmFactory::Create(
    const std::string& key_system,
    bool allow_distinctive_identifier,
    bool allow_persistent_state,
    const GURL& security_origin,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionErrorCB& session_error_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb) {
  // TODO(jrummell): Pass |security_origin| to all constructors.
  // TODO(jrummell): Enable the following line once blink code updated to
  // check the security origin before calling.
  // DCHECK(security_origin.is_valid());

  if (media::CanUseAesDecryptor(key_system)) {
    // TODO(sandersd): Currently the prefixed API always allows distinctive
    // identifiers and persistent state. Once that changes we can sanity check
    // here that neither is allowed for AesDecryptor, since it does not support
    // them and should never be configured that way. http://crbug.com/455271
    return scoped_ptr<media::MediaKeys>(new media::AesDecryptor(
        session_message_cb, session_closed_cb, session_keys_change_cb));
  }

#if defined(ENABLE_PEPPER_CDMS)
  return scoped_ptr<media::MediaKeys>(
      PpapiDecryptor::Create(key_system,
                             allow_distinctive_identifier,
                             allow_persistent_state,
                             security_origin,
                             create_pepper_cdm_cb_,
                             session_message_cb,
                             session_closed_cb,
                             session_error_cb,
                             session_keys_change_cb,
                             session_expiration_update_cb));
#elif defined(ENABLE_BROWSER_CDMS)
  DCHECK(allow_distinctive_identifier);
  DCHECK(allow_persistent_state);
  return scoped_ptr<media::MediaKeys>(
      ProxyMediaKeys::Create(key_system,
                             security_origin,
                             manager_,
                             session_message_cb,
                             session_closed_cb,
                             session_error_cb,
                             session_keys_change_cb,
                             session_expiration_update_cb));
#else
  return nullptr;
#endif  // defined(ENABLE_PEPPER_CDMS)
}

}  // namespace content
