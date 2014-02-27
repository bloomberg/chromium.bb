// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_CONTENT_DECRYPTION_MODULE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_CONTENT_DECRYPTION_MODULE_FACTORY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/media_keys.h"

#if defined(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/crypto/pepper_cdm_wrapper.h"
#endif

class GURL;

namespace content {

class RendererMediaPlayerManager;

class ContentDecryptionModuleFactory {
 public:
  // |create_pepper_cdm_cb| will be called synchronously if necessary. The other
  // callbacks can be called asynchronously.
  static scoped_ptr<media::MediaKeys> Create(
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
      const media::SessionErrorCB& session_error_cb);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_CONTENT_DECRYPTION_MODULE_FACTORY_H_
