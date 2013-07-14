// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/decryptor.h"
#include "media/base/media_keys.h"

namespace WebKit {
#if defined(ENABLE_PEPPER_CDMS)
class WebFrame;
class WebMediaPlayerClient;
#endif  // defined(ENABLE_PEPPER_CDMS)
}

namespace content {

class WebMediaPlayerProxyAndroid;

// ProxyDecryptor is for EME v0.1b only. It should not be used for the WD API.
// A decryptor proxy that creates a real decryptor object on demand and
// forwards decryptor calls to it.
// TODO(xhwang): Currently we don't support run-time switching among decryptor
// objects. Fix this when needed.
// TODO(xhwang): The ProxyDecryptor is not a Decryptor. Find a better name!
class ProxyDecryptor : public media::MediaKeys {
 public:
  ProxyDecryptor(
#if defined(ENABLE_PEPPER_CDMS)
      WebKit::WebMediaPlayerClient* web_media_player_client,
      WebKit::WebFrame* web_frame,
#elif defined(OS_ANDROID)
      WebMediaPlayerProxyAndroid* proxy,
      int media_keys_id,
#endif  // defined(ENABLE_PEPPER_CDMS)
      const media::KeyAddedCB& key_added_cb,
      const media::KeyErrorCB& key_error_cb,
      const media::KeyMessageCB& key_message_cb);
  virtual ~ProxyDecryptor();

  // Only call this once.
  bool InitializeCDM(const std::string& key_system);

  // Requests the ProxyDecryptor to notify the decryptor when it's ready through
  // the |decryptor_ready_cb| provided.
  // If |decryptor_ready_cb| is null, the existing callback will be fired with
  // NULL immediately and reset.
  void SetDecryptorReadyCB(const media::DecryptorReadyCB& decryptor_ready_cb);

  // MediaKeys implementation.
  // May only be called after InitializeCDM() succeeds.
  virtual bool GenerateKeyRequest(const std::string& type,
                                  const uint8* init_data,
                                  int init_data_length) OVERRIDE;
  virtual void AddKey(const uint8* key, int key_length,
                      const uint8* init_data, int init_data_length,
                      const std::string& session_id) OVERRIDE;
  virtual void CancelKeyRequest(const std::string& session_id) OVERRIDE;

 private:
  // Helper function to create MediaKeys to handle the given |key_system|.
  scoped_ptr<media::MediaKeys> CreateMediaKeys(const std::string& key_system);

  // Callbacks for firing key events.
  void KeyAdded(const std::string& session_id);
  void KeyError(const std::string& session_id,
                media::MediaKeys::KeyError error_code,
                int system_code);
  void KeyMessage(const std::string& session_id,
                  const std::vector<uint8>& message,
                  const std::string& default_url);

  base::WeakPtrFactory<ProxyDecryptor> weak_ptr_factory_;

#if defined(ENABLE_PEPPER_CDMS)
  // Callback for cleaning up a Pepper-based CDM.
  void DestroyHelperPlugin();

  // Needed to create the PpapiDecryptor.
  WebKit::WebMediaPlayerClient* web_media_player_client_;
  WebKit::WebFrame* web_frame_;
#elif defined(OS_ANDROID)
  WebMediaPlayerProxyAndroid* proxy_;
  int media_keys_id_;
#endif  // defined(ENABLE_PEPPER_CDMS)

  // The real MediaKeys that manages key operations for the ProxyDecryptor.
  // This pointer is protected by the |lock_|.
  scoped_ptr<media::MediaKeys> media_keys_;

  // Callbacks for firing key events.
  media::KeyAddedCB key_added_cb_;
  media::KeyErrorCB key_error_cb_;
  media::KeyMessageCB key_message_cb_;

  // Protects the |decryptor_|. Note that |decryptor_| itself should be thread
  // safe as per the Decryptor interface.
  base::Lock lock_;

  media::DecryptorReadyCB decryptor_ready_cb_;

  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptor);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
