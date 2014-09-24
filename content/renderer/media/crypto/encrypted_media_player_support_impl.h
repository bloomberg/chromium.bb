// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_ENCRYPTED_MEDIA_PLAYER_SUPPORT_IMPL_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_ENCRYPTED_MEDIA_PLAYER_SUPPORT_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/renderer/media/crypto/proxy_decryptor.h"
#include "media/blink/encrypted_media_player_support.h"

namespace blink {
class WebMediaPlayerClient;
}

namespace content {

class WebContentDecryptionModuleImpl;

class EncryptedMediaPlayerSupportImpl
    : public media::EncryptedMediaPlayerSupport,
      public base::SupportsWeakPtr<EncryptedMediaPlayerSupportImpl> {
 public:
  static scoped_ptr<EncryptedMediaPlayerSupport> Create(
      blink::WebMediaPlayerClient* client);

  virtual ~EncryptedMediaPlayerSupportImpl();

  // EncryptedMediaPlayerSupport implementation.
  virtual blink::WebMediaPlayer::MediaKeyException GenerateKeyRequest(
      blink::WebLocalFrame* frame,
      const blink::WebString& key_system,
      const unsigned char* init_data,
      unsigned init_data_length)  OVERRIDE;

  virtual blink::WebMediaPlayer::MediaKeyException AddKey(
      const blink::WebString& key_system,
      const unsigned char* key,
      unsigned key_length,
      const unsigned char* init_data,
      unsigned init_data_length,
      const blink::WebString& session_id) OVERRIDE;

  virtual blink::WebMediaPlayer::MediaKeyException CancelKeyRequest(
      const blink::WebString& key_system,
      const blink::WebString& session_id) OVERRIDE;

  virtual void SetInitialContentDecryptionModule(
      blink::WebContentDecryptionModule* initial_cdm) OVERRIDE;

  virtual void SetContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm) OVERRIDE;
  virtual void SetContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm,
      blink::WebContentDecryptionModuleResult result) OVERRIDE;

  virtual media::SetDecryptorReadyCB CreateSetDecryptorReadyCB() OVERRIDE;
  virtual media::Demuxer::NeedKeyCB CreateNeedKeyCB() OVERRIDE;

  virtual void OnPipelineDecryptError() OVERRIDE;

 private:
  explicit EncryptedMediaPlayerSupportImpl(blink::WebMediaPlayerClient* client);

  // Requests that this object notifies when a decryptor is ready through the
  // |decryptor_ready_cb| provided.
  // If |decryptor_ready_cb| is null, the existing callback will be fired with
  // NULL immediately and reset.
  void SetDecryptorReadyCB(const media::DecryptorReadyCB& decryptor_ready_cb);

  blink::WebMediaPlayer::MediaKeyException GenerateKeyRequestInternal(
      blink::WebLocalFrame* frame,
      const std::string& key_system,
      const unsigned char* init_data,
      unsigned init_data_length);

  blink::WebMediaPlayer::MediaKeyException AddKeyInternal(
      const std::string& key_system,
      const unsigned char* key,
      unsigned key_length,
      const unsigned char* init_data,
      unsigned init_data_length,
      const std::string& session_id);

  blink::WebMediaPlayer::MediaKeyException CancelKeyRequestInternal(
    const std::string& key_system,
    const std::string& session_id);

  void OnNeedKey(const std::string& type,
                 const std::vector<uint8>& init_data);

  void OnKeyAdded(const std::string& session_id);
  void OnKeyError(const std::string& session_id,
                  media::MediaKeys::KeyError error_code,
                  uint32 system_code);
  void OnKeyMessage(const std::string& session_id,
                    const std::vector<uint8>& message,
                    const GURL& destination_url);

  void ContentDecryptionModuleAttached(
      blink::WebContentDecryptionModuleResult result,
      bool success);

  blink::WebMediaPlayerClient* client_;

  // The currently selected key system. Empty string means that no key system
  // has been selected.
  std::string current_key_system_;

  // Temporary for EME v0.1. In the future the init data type should be passed
  // through GenerateKeyRequest() directly from WebKit.
  std::string init_data_type_;

  // Manages decryption keys and decrypts encrypted frames.
  scoped_ptr<ProxyDecryptor> proxy_decryptor_;

  // Non-owned pointer to the CDM. Updated via calls to
  // setContentDecryptionModule().
  WebContentDecryptionModuleImpl* web_cdm_;

  media::DecryptorReadyCB decryptor_ready_cb_;

  DISALLOW_COPY_AND_ASSIGN(EncryptedMediaPlayerSupportImpl);
};
}

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_ENCRYPTED_MEDIA_PLAYER_SUPPORT_IMPL_H_
