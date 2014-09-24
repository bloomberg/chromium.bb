// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_ENCRYPTED_MEDIA_PLAYER_SUPPORT_H_
#define MEDIA_BLINK_ENCRYPTED_MEDIA_PLAYER_SUPPORT_H_

#include "media/base/decryptor.h"
#include "media/base/demuxer.h"
#include "media/base/media_export.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"

namespace blink {
class WebContentDecryptionModule;
class WebContentDecryptionModuleResult;
class WebLocalFrame;
class WebMediaPlayerClient;
class WebString;
}

namespace media {

class MEDIA_EXPORT EncryptedMediaPlayerSupport {
 public:
  EncryptedMediaPlayerSupport();
  virtual ~EncryptedMediaPlayerSupport();

  // Prefixed API methods.
  virtual blink::WebMediaPlayer::MediaKeyException GenerateKeyRequest(
      blink::WebLocalFrame* frame,
      const blink::WebString& key_system,
      const unsigned char* init_data,
      unsigned init_data_length) = 0;

  virtual blink::WebMediaPlayer::MediaKeyException AddKey(
      const blink::WebString& key_system,
      const unsigned char* key,
      unsigned key_length,
      const unsigned char* init_data,
      unsigned init_data_length,
      const blink::WebString& session_id) = 0;

  virtual blink::WebMediaPlayer::MediaKeyException CancelKeyRequest(
      const blink::WebString& key_system,
      const blink::WebString& session_id) = 0;


  // Unprefixed API methods.
  virtual void SetInitialContentDecryptionModule(
      blink::WebContentDecryptionModule* initial_cdm) = 0;
  virtual void SetContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm) = 0;
  virtual void SetContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm,
      blink::WebContentDecryptionModuleResult result) = 0;


  // Callback factory and notification methods used by WebMediaPlayerImpl.

  // Creates a callback that Demuxers can use to signal that the content
  // requires a key. This method make sure the callback returned can be safely
  // invoked from any thread.
  virtual Demuxer::NeedKeyCB CreateNeedKeyCB() = 0;

  // Creates a callback that renderers can use to set decryptor
  // ready callback. This method make sure the callback returned can be safely
  // invoked from any thread.
  virtual SetDecryptorReadyCB CreateSetDecryptorReadyCB() = 0;

  // Called to inform this object that the media pipeline encountered
  // and handled a decryption error.
  virtual void OnPipelineDecryptError() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(EncryptedMediaPlayerSupport);
};

}  // namespace media

#endif  // MEDIA_BLINK_ENCRYPTED_MEDIA_PLAYER_SUPPORT_H_
