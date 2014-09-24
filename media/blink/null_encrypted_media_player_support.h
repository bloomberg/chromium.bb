// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_NULL_ENCRYPTED_MEDIA_PLAYER_SUPPORT_H_
#define MEDIA_BLINK_NULL_ENCRYPTED_MEDIA_PLAYER_SUPPORT_H_

#include "media/base/media_export.h"
#include "media/blink/encrypted_media_player_support.h"

namespace media {

// A "null" implementation of the EncryptedMediaPlayerSupport interface
// that indicates all key systems are not supported. This makes sure that
// any attempts to play encrypted content always fail.
class MEDIA_EXPORT NullEncryptedMediaPlayerSupport
    : public EncryptedMediaPlayerSupport {
 public:
  static scoped_ptr<EncryptedMediaPlayerSupport> Create(
      blink::WebMediaPlayerClient* client);

  virtual ~NullEncryptedMediaPlayerSupport();

  // Prefixed API methods.
  virtual blink::WebMediaPlayer::MediaKeyException GenerateKeyRequest(
      blink::WebLocalFrame* frame,
      const blink::WebString& key_system,
      const unsigned char* init_data,
      unsigned init_data_length) OVERRIDE;

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


  // Unprefixed API methods.
  virtual void SetInitialContentDecryptionModule(
      blink::WebContentDecryptionModule* initial_cdm) OVERRIDE;
  virtual void SetContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm) OVERRIDE;
  virtual void SetContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm,
      blink::WebContentDecryptionModuleResult result) OVERRIDE;


  // Callback factory and notification methods used by WebMediaPlayerImpl.

  // Creates a callback that Demuxers can use to signal that the content
  // requires a key. This method makes sure the callback returned can be safely
  // invoked from any thread.
  virtual Demuxer::NeedKeyCB CreateNeedKeyCB() OVERRIDE;

  // Creates a callback that renderers can use to set decryptor
  // ready callback. This method makes sure the callback returned can be safely
  // invoked from any thread.
  virtual SetDecryptorReadyCB CreateSetDecryptorReadyCB() OVERRIDE;

  // Called to inform this object that the media pipeline encountered
  // and handled a decryption error.
  virtual void OnPipelineDecryptError() OVERRIDE;

 private:
  NullEncryptedMediaPlayerSupport();

  DISALLOW_COPY_AND_ASSIGN(NullEncryptedMediaPlayerSupport);
};

}  // namespace media

#endif  // MEDIA_BLINK_NULL_ENCRYPTED_MEDIA_PLAYER_SUPPORT_H_
