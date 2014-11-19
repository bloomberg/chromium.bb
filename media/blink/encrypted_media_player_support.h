// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_ENCRYPTED_MEDIA_PLAYER_SUPPORT_H_
#define MEDIA_BLINK_ENCRYPTED_MEDIA_PLAYER_SUPPORT_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_factory.h"
#include "media/base/demuxer.h"
#include "media/cdm/proxy_decryptor.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"

namespace blink {
class WebContentDecryptionModule;
class WebLocalFrame;
class WebMediaPlayerClient;
class WebString;
}

namespace media {

class WebContentDecryptionModuleImpl;

class EncryptedMediaPlayerSupport
    : public base::SupportsWeakPtr<EncryptedMediaPlayerSupport> {
 public:
  EncryptedMediaPlayerSupport(scoped_ptr<CdmFactory> cdm_factory,
                              blink::WebMediaPlayerClient* client,
                              blink::WebContentDecryptionModule* initial_cdm);
  ~EncryptedMediaPlayerSupport();

  blink::WebMediaPlayer::MediaKeyException GenerateKeyRequest(
      blink::WebLocalFrame* frame,
      const blink::WebString& key_system,
      const unsigned char* init_data,
      unsigned init_data_length);

  blink::WebMediaPlayer::MediaKeyException AddKey(
      const blink::WebString& key_system,
      const unsigned char* key,
      unsigned key_length,
      const unsigned char* init_data,
      unsigned init_data_length,
      const blink::WebString& session_id);

  blink::WebMediaPlayer::MediaKeyException CancelKeyRequest(
      const blink::WebString& key_system,
      const blink::WebString& session_id);

  void SetContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm);
  void SetContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm,
      blink::WebContentDecryptionModuleResult result);

  SetDecryptorReadyCB CreateSetDecryptorReadyCB();
  Demuxer::NeedKeyCB CreateNeedKeyCB();

  void OnPipelineDecryptError();

 private:
  // Requests that this object notifies when a decryptor is ready through the
  // |decryptor_ready_cb| provided.
  // If |decryptor_ready_cb| is null, the existing callback will be fired with
  // NULL immediately and reset.
  void SetDecryptorReadyCallback(const DecryptorReadyCB& decryptor_ready_cb);

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
                  MediaKeys::KeyError error_code,
                  uint32 system_code);
  void OnKeyMessage(const std::string& session_id,
                    const std::vector<uint8>& message,
                    const GURL& destination_url);

  void ContentDecryptionModuleAttached(
      blink::WebContentDecryptionModuleResult result,
      bool success);

  scoped_ptr<CdmFactory> cdm_factory_;

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

  DecryptorReadyCB decryptor_ready_cb_;

  DISALLOW_COPY_AND_ASSIGN(EncryptedMediaPlayerSupport);
};

}  // namespace media

#endif  // MEDIA_BLINK_ENCRYPTED_MEDIA_PLAYER_SUPPORT_H_
