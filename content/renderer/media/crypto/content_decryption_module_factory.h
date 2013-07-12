// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_CONTENT_DECRYPTION_MODULE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_CONTENT_DECRYPTION_MODULE_FACTORY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/media_keys.h"

#if defined(ENABLE_PEPPER_CDMS)
namespace WebKit {
class WebFrame;
class WebMediaPlayerClient;
}
#endif  // defined(ENABLE_PEPPER_CDMS)

namespace content {

class ContentDecryptionModuleFactory {
 public:
  static scoped_ptr<media::MediaKeys> Create(
      const std::string& key_system,
#if defined(ENABLE_PEPPER_CDMS)
      // TODO(ddorwin): We need different pointers for the WD API.
      WebKit::WebMediaPlayerClient* web_media_player_client,
      WebKit::WebFrame* web_frame,
      const base::Closure& destroy_plugin_cb,
#elif defined(OS_ANDROID)
      // TODO(scherkus): Revert the ProxyDecryptor changes from r208040 so that
      // this class always creates the MediaKeys.
      // A ProxyMediaKeys to be used if |key_system| is not Clear Key.
      scoped_ptr<media::MediaKeys> proxy_media_keys,
#endif  // defined(ENABLE_PEPPER_CDMS)
      const media::KeyAddedCB& key_added_cb,
      const media::KeyErrorCB& key_error_cb,
      const media::KeyMessageCB& key_message_cb);

#if defined(ENABLE_PEPPER_CDMS)
  static void DestroyHelperPlugin(
      WebKit::WebMediaPlayerClient* web_media_player_client);
#endif  // defined(ENABLE_PEPPER_CDMS)
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_CONTENT_DECRYPTION_MODULE_FACTORY_H_
