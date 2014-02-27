// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/content_decryption_module_factory.h"

#include "base/logging.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "media/cdm/aes_decryptor.h"

#if defined(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/crypto/ppapi_decryptor.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_webplugin_impl.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#elif defined(OS_ANDROID)
#include "content/renderer/media/android/proxy_media_keys.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#endif  // defined(ENABLE_PEPPER_CDMS)

namespace content {

scoped_ptr<media::MediaKeys> ContentDecryptionModuleFactory::Create(
    const std::string& key_system,
#if defined(ENABLE_PEPPER_CDMS)
    const CreatePepperCdmCB& create_pepper_cdm_cb,
#elif defined(OS_ANDROID)
    RendererMediaPlayerManager* manager,
    int media_keys_id,
    const GURL& frame_url,
#endif  // defined(ENABLE_PEPPER_CDMS)
    const media::SessionCreatedCB& session_created_cb,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionReadyCB& session_ready_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionErrorCB& session_error_cb) {
  if (CanUseAesDecryptor(key_system)) {
    return scoped_ptr<media::MediaKeys>(
        new media::AesDecryptor(session_created_cb,
                                session_message_cb,
                                session_ready_cb,
                                session_closed_cb,
                                session_error_cb));
  }

#if defined(ENABLE_PEPPER_CDMS)
  return scoped_ptr<media::MediaKeys>(
      PpapiDecryptor::Create(key_system,
                             create_pepper_cdm_cb,
                             session_created_cb,
                             session_message_cb,
                             session_ready_cb,
                             session_closed_cb,
                             session_error_cb));
#elif defined(OS_ANDROID)
  scoped_ptr<ProxyMediaKeys> proxy_media_keys(
      new ProxyMediaKeys(manager,
                         media_keys_id,
                         session_created_cb,
                         session_message_cb,
                         session_ready_cb,
                         session_closed_cb,
                         session_error_cb));
  proxy_media_keys->InitializeCDM(key_system, frame_url);
  return proxy_media_keys.PassAs<media::MediaKeys>();
#else
  return scoped_ptr<media::MediaKeys>();
#endif  // defined(ENABLE_PEPPER_CDMS)
}

}  // namespace content
