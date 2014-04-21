// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/encrypted_media_message_filter_android.h"

#include <string>

#include "chrome/common/encrypted_media_messages_android.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_drm_bridge.h"

using content::BrowserThread;
using media::MediaCodecBridge;
using media::MediaDrmBridge;

namespace chrome {

const size_t kMaxKeySystemLength = 256;

enum CodecType {
  CODEC_AUDIO,
  CODEC_VIDEO
};

struct CodecInfo {
  android::SupportedCodecs codec;
  CodecType codec_type;
  const char* codec_name;
  const char* container_mime_type;
};

const CodecInfo kCodecsToQuery[] = {
  {android::WEBM_VORBIS, CODEC_AUDIO, "vorbis", "video/webm"},
  {android::WEBM_VP8, CODEC_VIDEO, "vp8", "video/webm"},
#if defined(USE_PROPRIETARY_CODECS)
  {android::MP4_AAC, CODEC_AUDIO, "mp4a", "video/mp4"},
  {android::MP4_AVC1, CODEC_VIDEO, "avc1", "video/mp4"}
#endif  // defined(USE_PROPRIETARY_CODECS)
};

static android::SupportedCodecs GetSupportedCodecs(
    const SupportedKeySystemRequest& request,
    bool video_must_be_compositable) {
  const std::string& key_system = request.key_system;
  android::SupportedCodecs supported_codecs = android::NO_CODECS;

  for (size_t i = 0; i < arraysize(kCodecsToQuery); ++i) {
    const CodecInfo& info = kCodecsToQuery[i];
    // TODO(qinmin): Remove the composition logic when secure contents can be
    // composited.
    bool is_secure = (info.codec_type == CODEC_VIDEO)
                         ? (!video_must_be_compositable) : false;
    if ((request.codecs & info.codec) &&
        MediaDrmBridge::IsKeySystemSupportedWithType(
            key_system, info.container_mime_type) &&
        MediaCodecBridge::CanDecode(info.codec_name, is_secure)) {
      supported_codecs = static_cast<android::SupportedCodecs>(
          supported_codecs | info.codec);
    }
  }

  return supported_codecs;
}

EncryptedMediaMessageFilterAndroid::EncryptedMediaMessageFilterAndroid()
    : BrowserMessageFilter(EncryptedMediaMsgStart) {}

EncryptedMediaMessageFilterAndroid::~EncryptedMediaMessageFilterAndroid() {}

bool EncryptedMediaMessageFilterAndroid::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(
      EncryptedMediaMessageFilterAndroid, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_GetSupportedKeySystems,
                        OnGetSupportedKeySystems)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void EncryptedMediaMessageFilterAndroid::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  // Move the IPC handling to FILE thread as it is not very cheap.
  if (message.type() == ChromeViewHostMsg_GetSupportedKeySystems::ID)
    *thread = BrowserThread::FILE;
}

void EncryptedMediaMessageFilterAndroid::OnGetSupportedKeySystems(
    const SupportedKeySystemRequest& request,
    SupportedKeySystemResponse* response) {
  if (!response) {
    NOTREACHED() << "NULL response pointer provided.";
    return;
  }

  if (request.key_system.size() > kMaxKeySystemLength) {
    NOTREACHED() << "Invalid key system: " << request.key_system;
    return;
  }

  if (!MediaDrmBridge::IsKeySystemSupported(request.key_system))
    return;

  DCHECK_EQ(request.codecs & android::INVALID_CODECS, android::NO_CODECS)
      << "unrecognized codec";
  response->key_system = request.key_system;
  // TODO(qinmin): check composition is supported or not.
  response->compositing_codecs = GetSupportedCodecs(request, true);
  response->non_compositing_codecs = GetSupportedCodecs(request, false);
}

}  // namespace chrome
