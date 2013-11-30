// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_CONTENT_DECRYPTION_MODULE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_CONTENT_DECRYPTION_MODULE_FACTORY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/media_keys.h"

class GURL;

#if defined(ENABLE_PEPPER_CDMS)
namespace blink {
class WebFrame;
class WebMediaPlayerClient;
}
#endif  // defined(ENABLE_PEPPER_CDMS)

namespace content {

class RendererMediaPlayerManager;

class ContentDecryptionModuleFactory {
 public:
  static scoped_ptr<media::MediaKeys> Create(
      const std::string& key_system,
#if defined(ENABLE_PEPPER_CDMS)
      // TODO(ddorwin): We need different pointers for the WD API.
      blink::WebMediaPlayerClient* web_media_player_client,
      blink::WebFrame* web_frame,
      const base::Closure& destroy_plugin_cb,
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

#if defined(ENABLE_PEPPER_CDMS)
  static void DestroyHelperPlugin(
      blink::WebMediaPlayerClient* web_media_player_client,
      blink::WebFrame* web_frame);
#endif  // defined(ENABLE_PEPPER_CDMS)
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_CONTENT_DECRYPTION_MODULE_FACTORY_H_
