// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/null_encrypted_media_player_support.h"

#include "base/bind.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModule.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"

namespace media {

static void NeedKeyHandler(const std::string& type,
                           const std::vector<uint8>& init_data) {
  NOTIMPLEMENTED();
}

scoped_ptr<EncryptedMediaPlayerSupport>
NullEncryptedMediaPlayerSupport::Create(blink::WebMediaPlayerClient* client) {
  return scoped_ptr<EncryptedMediaPlayerSupport>(
      new NullEncryptedMediaPlayerSupport());
}

NullEncryptedMediaPlayerSupport::NullEncryptedMediaPlayerSupport() {
}

NullEncryptedMediaPlayerSupport::~NullEncryptedMediaPlayerSupport() {
}

blink::WebMediaPlayer::MediaKeyException
NullEncryptedMediaPlayerSupport::GenerateKeyRequest(
    blink::WebLocalFrame* frame,
    const blink::WebString& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  return blink::WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;
}

blink::WebMediaPlayer::MediaKeyException
NullEncryptedMediaPlayerSupport::AddKey(
    const blink::WebString& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const blink::WebString& session_id) {
  return blink::WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;
}

blink::WebMediaPlayer::MediaKeyException
NullEncryptedMediaPlayerSupport::CancelKeyRequest(
    const blink::WebString& key_system,
    const blink::WebString& session_id) {
  return blink::WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;
}

void NullEncryptedMediaPlayerSupport::SetInitialContentDecryptionModule(
    blink::WebContentDecryptionModule* initial_cdm) {
}

void NullEncryptedMediaPlayerSupport::SetContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm) {
}

void NullEncryptedMediaPlayerSupport::SetContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm,
    blink::WebContentDecryptionModuleResult result) {
  result.completeWithError(
      blink::WebContentDecryptionModuleExceptionNotSupportedError,
      0,
      "Null MediaKeys object is not supported.");
}

Demuxer::NeedKeyCB NullEncryptedMediaPlayerSupport::CreateNeedKeyCB() {
  return base::Bind(&NeedKeyHandler);
}

SetDecryptorReadyCB
NullEncryptedMediaPlayerSupport::CreateSetDecryptorReadyCB() {
  return SetDecryptorReadyCB();
}

void NullEncryptedMediaPlayerSupport::OnPipelineDecryptError() {
}

}  // namespace media
