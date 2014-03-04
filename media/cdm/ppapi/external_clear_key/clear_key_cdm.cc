// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/ppapi/external_clear_key/clear_key_cdm.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/cdm/json_web_key.h"
#include "media/cdm/ppapi/cdm_file_io_test.h"
#include "media/cdm/ppapi/external_clear_key/cdm_video_decoder.h"

#if defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
#include "base/basictypes.h"
const int64 kNoTimestamp = kint64min;
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
#include "base/at_exit.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "media/base/media.h"
#include "media/cdm/ppapi/external_clear_key/ffmpeg_cdm_audio_decoder.h"
#include "media/cdm/ppapi/external_clear_key/ffmpeg_cdm_video_decoder.h"

// Include FFmpeg avformat.h for av_register_all().
extern "C" {
// Temporarily disable possible loss of data warning.
MSVC_PUSH_DISABLE_WARNING(4244);
#include <libavformat/avformat.h>
MSVC_POP_WARNING();
}  // extern "C"

// TODO(tomfinegan): When COMPONENT_BUILD is not defined an AtExitManager must
// exist before the call to InitializeFFmpegLibraries(). This should no longer
// be required after http://crbug.com/91970 because we'll be able to get rid of
// InitializeFFmpegLibraries().
#if !defined COMPONENT_BUILD
static base::AtExitManager g_at_exit_manager;
#endif

// TODO(tomfinegan): InitializeFFmpegLibraries() and |g_cdm_module_initialized|
// are required for running in the sandbox, and should no longer be required
// after http://crbug.com/91970 is fixed.
static bool InitializeFFmpegLibraries() {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_MODULE, &file_path));
  CHECK(media::InitializeMediaLibrary(file_path));
  return true;
}

static bool g_ffmpeg_lib_initialized = InitializeFFmpegLibraries();
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER

const char kClearKeyCdmVersion[] = "0.1.0.1";
const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";
const char kExternalClearKeyDecryptOnlyKeySystem[] =
    "org.chromium.externalclearkey.decryptonly";
const char kExternalClearKeyFileIOTestKeySystem[] =
    "org.chromium.externalclearkey.fileiotest";
const char kExternalClearKeyCrashKeySystem[] =
    "org.chromium.externalclearkey.crash";

// Constants for the enumalted session that can be loaded by LoadSession().
// These constants need to be in sync with
// chrome/test/data/media/encrypted_media_utils.js
const char kLoadableWebSessionId[] = "LoadableSession";
const char kLoadableSessionContentType[] = "video/webm";
const uint8 kLoadableSessionKeyId[] = "0123456789012345";
const uint8 kLoadableSessionKey[] =
    {0xeb, 0xdd, 0x62, 0xf1, 0x68, 0x14, 0xd2, 0x7b,
     0x68, 0xef, 0x12, 0x2a, 0xfc, 0xe4, 0xae, 0x3c};

const int64 kSecondsPerMinute = 60;
const int64 kMsPerSecond = 1000;
const int64 kInitialTimerDelayMs = 200;
const int64 kMaxTimerDelayMs = 1 * kSecondsPerMinute * kMsPerSecond;
// Heart beat message header. If a key message starts with |kHeartBeatHeader|,
// it's a heart beat message. Otherwise, it's a key request.
const char kHeartBeatHeader[] = "HEARTBEAT";
// CDM file IO test result header.
const char kFileIOTestResultHeader[] = "FILEIOTESTRESULT";

// Copies |input_buffer| into a media::DecoderBuffer. If the |input_buffer| is
// empty, an empty (end-of-stream) media::DecoderBuffer is returned.
static scoped_refptr<media::DecoderBuffer> CopyDecoderBufferFrom(
    const cdm::InputBuffer& input_buffer) {
  if (!input_buffer.data) {
    DCHECK(!input_buffer.data_size);
    return media::DecoderBuffer::CreateEOSBuffer();
  }

  // TODO(xhwang): Get rid of this copy.
  scoped_refptr<media::DecoderBuffer> output_buffer =
      media::DecoderBuffer::CopyFrom(input_buffer.data, input_buffer.data_size);

  std::vector<media::SubsampleEntry> subsamples;
  for (uint32_t i = 0; i < input_buffer.num_subsamples; ++i) {
    media::SubsampleEntry subsample;
    subsample.clear_bytes = input_buffer.subsamples[i].clear_bytes;
    subsample.cypher_bytes = input_buffer.subsamples[i].cipher_bytes;
    subsamples.push_back(subsample);
  }

  DCHECK_EQ(input_buffer.data_offset, 0u);
  scoped_ptr<media::DecryptConfig> decrypt_config(new media::DecryptConfig(
      std::string(reinterpret_cast<const char*>(input_buffer.key_id),
                  input_buffer.key_id_size),
      std::string(reinterpret_cast<const char*>(input_buffer.iv),
                  input_buffer.iv_size),
      subsamples));

  output_buffer->set_decrypt_config(decrypt_config.Pass());
  output_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(input_buffer.timestamp));

  return output_buffer;
}

static std::string GetFileIOTestResultMessage(bool success) {
  std::string message(kFileIOTestResultHeader);
  message += success ? '1' : '0';
  return message;
}

template<typename Type>
class ScopedResetter {
 public:
  explicit ScopedResetter(Type* object) : object_(object) {}
  ~ScopedResetter() { object_->Reset(); }

 private:
  Type* const object_;
};

void INITIALIZE_CDM_MODULE() {
#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  DVLOG(2) << "FFmpeg libraries initialized: " << g_ffmpeg_lib_initialized;
  av_register_all();
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

void DeinitializeCdmModule() {
}

void* CreateCdmInstance(int cdm_interface_version,
                        const char* key_system, uint32_t key_system_size,
                        GetCdmHostFunc get_cdm_host_func,
                        void* user_data) {
  DVLOG(1) << "CreateCdmInstance()";

  std::string key_system_string(key_system, key_system_size);
  if (key_system_string != kExternalClearKeyKeySystem &&
      key_system_string != kExternalClearKeyDecryptOnlyKeySystem &&
      key_system_string != kExternalClearKeyFileIOTestKeySystem &&
      key_system_string != kExternalClearKeyCrashKeySystem) {
    DVLOG(1) << "Unsupported key system:" << key_system_string;
    return NULL;
  }

  if (cdm_interface_version != media::ClearKeyCdmInterface::kVersion)
    return NULL;

  media::ClearKeyCdmHost* host = static_cast<media::ClearKeyCdmHost*>(
      get_cdm_host_func(media::ClearKeyCdmHost::kVersion, user_data));
  if (!host)
    return NULL;

  return new media::ClearKeyCdm(host, key_system_string);
}

const char* GetCdmVersion() {
  return kClearKeyCdmVersion;
}

namespace media {

ClearKeyCdm::ClearKeyCdm(ClearKeyCdmHost* host, const std::string& key_system)
    : decryptor_(
          base::Bind(&ClearKeyCdm::OnSessionCreated, base::Unretained(this)),
          base::Bind(&ClearKeyCdm::OnSessionMessage, base::Unretained(this)),
          base::Bind(&ClearKeyCdm::OnSessionReady, base::Unretained(this)),
          base::Bind(&ClearKeyCdm::OnSessionClosed, base::Unretained(this)),
          base::Bind(&ClearKeyCdm::OnSessionError, base::Unretained(this))),
      host_(host),
      key_system_(key_system),
      last_session_id_(MediaKeys::kInvalidSessionId),
      session_id_for_emulated_loadsession_(MediaKeys::kInvalidSessionId),
      timer_delay_ms_(kInitialTimerDelayMs),
      heartbeat_timer_set_(false) {
#if defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  channel_count_ = 0;
  bits_per_channel_ = 0;
  samples_per_second_ = 0;
  output_timestamp_base_in_microseconds_ = kNoTimestamp;
  total_samples_generated_ = 0;
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER
}

ClearKeyCdm::~ClearKeyCdm() {}

void ClearKeyCdm::CreateSession(uint32 session_id,
                                const char* type,
                                uint32 type_size,
                                const uint8* init_data,
                                uint32 init_data_size) {
  DVLOG(1) << __FUNCTION__;
  decryptor_.CreateSession(
      session_id, std::string(type, type_size), init_data, init_data_size);

  // Save the latest session ID for heartbeat and file IO test messages.
  last_session_id_ = session_id;

  if (key_system_ == kExternalClearKeyFileIOTestKeySystem)
    StartFileIOTest();
}

// Loads a emulated stored session. Currently only |kLoadableWebSessionId|
// (containing a |kLoadableSessionKey| for |kLoadableSessionKeyId|) is
// supported.
void ClearKeyCdm::LoadSession(uint32_t session_id,
                              const char* web_session_id,
                              uint32_t web_session_id_length) {
  DVLOG(1) << __FUNCTION__;

  if (std::string(kLoadableWebSessionId) !=
      std::string(web_session_id, web_session_id_length)) {
    // TODO(xhwang): Report "NotFoundError" when we support DOMError style.
    OnSessionError(session_id, MediaKeys::kUnknownError, 0);
    return;
  }

  session_id_for_emulated_loadsession_ = session_id;

  decryptor_.CreateSession(session_id, kLoadableSessionContentType, NULL, 0);
}

void ClearKeyCdm::UpdateSession(uint32 session_id,
                                const uint8* response,
                                uint32 response_size) {
  DVLOG(1) << __FUNCTION__;
  decryptor_.UpdateSession(session_id, response, response_size);

  if (!heartbeat_timer_set_) {
    ScheduleNextHeartBeat();
    heartbeat_timer_set_ = true;
  }
}

void ClearKeyCdm::ReleaseSession(uint32 session_id) {
  DVLOG(1) << __FUNCTION__;
  decryptor_.ReleaseSession(session_id);
}

void ClearKeyCdm::TimerExpired(void* context) {
  if (context == &session_id_for_emulated_loadsession_) {
    LoadLoadableSession();
    return;
  }

  DCHECK(heartbeat_timer_set_);
  std::string heartbeat_message;
  if (!next_heartbeat_message_.empty() &&
      context == &next_heartbeat_message_[0]) {
    heartbeat_message = next_heartbeat_message_;
  } else {
    heartbeat_message = "ERROR: Invalid timer context found!";
  }

  // This URL is only used for testing the code path for defaultURL.
  // There is no service at this URL, so applications should ignore it.
  const char url[] = "http://test.externalclearkey.chromium.org";

  host_->OnSessionMessage(last_session_id_,
                          heartbeat_message.data(),
                          heartbeat_message.size(),
                          url,
                          arraysize(url) - 1);

  ScheduleNextHeartBeat();
}

static void CopyDecryptResults(
    media::Decryptor::Status* status_copy,
    scoped_refptr<media::DecoderBuffer>* buffer_copy,
    media::Decryptor::Status status,
    const scoped_refptr<media::DecoderBuffer>& buffer) {
  *status_copy = status;
  *buffer_copy = buffer;
}

cdm::Status ClearKeyCdm::Decrypt(
    const cdm::InputBuffer& encrypted_buffer,
    cdm::DecryptedBlock* decrypted_block) {
  DVLOG(1) << "Decrypt()";
  DCHECK(encrypted_buffer.data);

  scoped_refptr<media::DecoderBuffer> buffer;
  cdm::Status status = DecryptToMediaDecoderBuffer(encrypted_buffer, &buffer);

  if (status != cdm::kSuccess)
    return status;

  DCHECK(buffer->data());
  decrypted_block->SetDecryptedBuffer(
      host_->Allocate(buffer->data_size()));
  memcpy(reinterpret_cast<void*>(decrypted_block->DecryptedBuffer()->Data()),
         buffer->data(),
         buffer->data_size());
  decrypted_block->DecryptedBuffer()->SetSize(buffer->data_size());
  decrypted_block->SetTimestamp(buffer->timestamp().InMicroseconds());

  return cdm::kSuccess;
}

cdm::Status ClearKeyCdm::InitializeAudioDecoder(
    const cdm::AudioDecoderConfig& audio_decoder_config) {
  if (key_system_ == kExternalClearKeyDecryptOnlyKeySystem)
    return cdm::kSessionError;

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  if (!audio_decoder_)
    audio_decoder_.reset(new media::FFmpegCdmAudioDecoder(host_));

  if (!audio_decoder_->Initialize(audio_decoder_config))
    return cdm::kSessionError;

  return cdm::kSuccess;
#elif defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  channel_count_ = audio_decoder_config.channel_count;
  bits_per_channel_ = audio_decoder_config.bits_per_channel;
  samples_per_second_ = audio_decoder_config.samples_per_second;
  return cdm::kSuccess;
#else
  NOTIMPLEMENTED();
  return cdm::kSessionError;
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

cdm::Status ClearKeyCdm::InitializeVideoDecoder(
    const cdm::VideoDecoderConfig& video_decoder_config) {
  if (key_system_ == kExternalClearKeyDecryptOnlyKeySystem)
    return cdm::kSessionError;

  if (video_decoder_ && video_decoder_->is_initialized()) {
    DCHECK(!video_decoder_->is_initialized());
    return cdm::kSessionError;
  }

  // Any uninitialized decoder will be replaced.
  video_decoder_ = CreateVideoDecoder(host_, video_decoder_config);
  if (!video_decoder_)
    return cdm::kSessionError;

  return cdm::kSuccess;
}

void ClearKeyCdm::ResetDecoder(cdm::StreamType decoder_type) {
  DVLOG(1) << "ResetDecoder()";
#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  switch (decoder_type) {
    case cdm::kStreamTypeVideo:
      video_decoder_->Reset();
      break;
    case cdm::kStreamTypeAudio:
      audio_decoder_->Reset();
      break;
    default:
      NOTREACHED() << "ResetDecoder(): invalid cdm::StreamType";
  }
#elif defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  if (decoder_type == cdm::kStreamTypeAudio) {
    output_timestamp_base_in_microseconds_ = kNoTimestamp;
    total_samples_generated_ = 0;
  }
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

void ClearKeyCdm::DeinitializeDecoder(cdm::StreamType decoder_type) {
  DVLOG(1) << "DeinitializeDecoder()";
  switch (decoder_type) {
    case cdm::kStreamTypeVideo:
      video_decoder_->Deinitialize();
      break;
    case cdm::kStreamTypeAudio:
#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
      audio_decoder_->Deinitialize();
#elif defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
      output_timestamp_base_in_microseconds_ = kNoTimestamp;
      total_samples_generated_ = 0;
#endif
      break;
    default:
      NOTREACHED() << "DeinitializeDecoder(): invalid cdm::StreamType";
  }
}

cdm::Status ClearKeyCdm::DecryptAndDecodeFrame(
    const cdm::InputBuffer& encrypted_buffer,
    cdm::VideoFrame* decoded_frame) {
  DVLOG(1) << "DecryptAndDecodeFrame()";
  TRACE_EVENT0("media", "ClearKeyCdm::DecryptAndDecodeFrame");

  scoped_refptr<media::DecoderBuffer> buffer;
  cdm::Status status = DecryptToMediaDecoderBuffer(encrypted_buffer, &buffer);

  if (status != cdm::kSuccess)
    return status;

  const uint8_t* data = NULL;
  int32_t size = 0;
  int64_t timestamp = 0;
  if (!buffer->end_of_stream()) {
    data = buffer->data();
    size = buffer->data_size();
    timestamp = encrypted_buffer.timestamp;
  }

  return video_decoder_->DecodeFrame(data, size, timestamp, decoded_frame);
}

cdm::Status ClearKeyCdm::DecryptAndDecodeSamples(
    const cdm::InputBuffer& encrypted_buffer,
    cdm::AudioFrames* audio_frames) {
  DVLOG(1) << "DecryptAndDecodeSamples()";

  // Trigger a crash on purpose for testing purpose.
  if (key_system_ == kExternalClearKeyCrashKeySystem)
    CHECK(false);

  scoped_refptr<media::DecoderBuffer> buffer;
  cdm::Status status = DecryptToMediaDecoderBuffer(encrypted_buffer, &buffer);

  if (status != cdm::kSuccess)
    return status;

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  const uint8_t* data = NULL;
  int32_t size = 0;
  int64_t timestamp = 0;
  if (!buffer->end_of_stream()) {
    data = buffer->data();
    size = buffer->data_size();
    timestamp = encrypted_buffer.timestamp;
  }

  return audio_decoder_->DecodeBuffer(data, size, timestamp, audio_frames);
#elif defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  int64 timestamp_in_microseconds = kNoTimestamp;
  if (!buffer->end_of_stream()) {
    timestamp_in_microseconds = buffer->GetTimestamp().InMicroseconds();
    DCHECK(timestamp_in_microseconds != kNoTimestamp);
  }
  return GenerateFakeAudioFrames(timestamp_in_microseconds, audio_frames);
#else
  return cdm::kSuccess;
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER
}

void ClearKeyCdm::Destroy() {
  DVLOG(1) << "Destroy()";
  delete this;
}

void ClearKeyCdm::ScheduleNextHeartBeat() {
  // Prepare the next heartbeat message and set timer.
  std::ostringstream msg_stream;
  msg_stream << kHeartBeatHeader << " from ClearKey CDM set at time "
             << host_->GetCurrentWallTimeInSeconds() << ".";
  next_heartbeat_message_ = msg_stream.str();

  host_->SetTimer(timer_delay_ms_, &next_heartbeat_message_[0]);

  // Use a smaller timer delay at start-up to facilitate testing. Increase the
  // timer delay up to a limit to avoid message spam.
  if (timer_delay_ms_ < kMaxTimerDelayMs)
    timer_delay_ms_ = std::min(2 * timer_delay_ms_, kMaxTimerDelayMs);
}

cdm::Status ClearKeyCdm::DecryptToMediaDecoderBuffer(
    const cdm::InputBuffer& encrypted_buffer,
    scoped_refptr<media::DecoderBuffer>* decrypted_buffer) {
  DCHECK(decrypted_buffer);
  scoped_refptr<media::DecoderBuffer> buffer =
      CopyDecoderBufferFrom(encrypted_buffer);

  if (buffer->end_of_stream()) {
    *decrypted_buffer = buffer;
    return cdm::kSuccess;
  }

  // Callback is called synchronously, so we can use variables on the stack.
  media::Decryptor::Status status = media::Decryptor::kError;
  // The AesDecryptor does not care what the stream type is. Pass kVideo
  // for both audio and video decryption.
  decryptor_.Decrypt(
      media::Decryptor::kVideo,
      buffer,
      base::Bind(&CopyDecryptResults, &status, decrypted_buffer));

  if (status == media::Decryptor::kError)
    return cdm::kDecryptError;

  if (status == media::Decryptor::kNoKey)
    return cdm::kNoKey;

  DCHECK_EQ(status, media::Decryptor::kSuccess);
  return cdm::kSuccess;
}

void ClearKeyCdm::OnPlatformChallengeResponse(
    const cdm::PlatformChallengeResponse& response) {
  NOTIMPLEMENTED();
}

void ClearKeyCdm::OnQueryOutputProtectionStatus(
    uint32_t link_mask, uint32_t output_protection_mask) {
  NOTIMPLEMENTED();
};

void ClearKeyCdm::LoadLoadableSession() {
  std::string jwk_set = GenerateJWKSet(kLoadableSessionKey,
                                       sizeof(kLoadableSessionKey),
                                       kLoadableSessionKeyId,
                                       sizeof(kLoadableSessionKeyId) - 1);
  // TODO(xhwang): This triggers OnSessionUpdated(). For prefixed EME support,
  // this is okay. Check WD EME support.
  decryptor_.UpdateSession(session_id_for_emulated_loadsession_,
                           reinterpret_cast<const uint8*>(jwk_set.data()),
                           jwk_set.size());
}

void ClearKeyCdm::OnSessionCreated(uint32 session_id,
                                   const std::string& web_session_id) {
  std::string new_web_session_id = web_session_id;

  if (session_id == session_id_for_emulated_loadsession_) {
    // Delay LoadLoadableSession() to test the case where Decrypt*() calls are
    // made before the session is fully loaded.
    const int64 kDelayToLoadSessionMs = 500;
    // Use the address of |session_id_for_emulated_loadsession_| as the timer
    // context so that we can call LoadLoadableSession() when the timer expires.
    host_->SetTimer(kDelayToLoadSessionMs,
                    &session_id_for_emulated_loadsession_);
    // Defer OnSessionCreated() until the session is loaded.
    return;
  }

  host_->OnSessionCreated(
      session_id, web_session_id.data(), web_session_id.size());
}

void ClearKeyCdm::OnSessionMessage(uint32 session_id,
                                   const std::vector<uint8>& message,
                                   const std::string& destination_url) {
  DVLOG(1) << "OnSessionMessage: " << message.size();

  // Ignore the message when we are waiting to update the loadable session.
  if (session_id == session_id_for_emulated_loadsession_)
    return;

  host_->OnSessionMessage(session_id,
                          reinterpret_cast<const char*>(message.data()),
                          message.size(),
                          destination_url.data(),
                          destination_url.size());
}

void ClearKeyCdm::OnSessionReady(uint32 session_id) {
  if (session_id == session_id_for_emulated_loadsession_) {
    session_id_for_emulated_loadsession_ = MediaKeys::kInvalidSessionId;
    host_->OnSessionCreated(
        session_id, kLoadableWebSessionId, strlen(kLoadableWebSessionId));
  }

  host_->OnSessionReady(session_id);
}

void ClearKeyCdm::OnSessionClosed(uint32 session_id) {
  host_->OnSessionClosed(session_id);
}

void ClearKeyCdm::OnSessionError(uint32 session_id,
                                 media::MediaKeys::KeyError error_code,
                                 uint32 system_code) {
  host_->OnSessionError(
      session_id, static_cast<cdm::MediaKeyError>(error_code), system_code);
}

#if defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
int64 ClearKeyCdm::CurrentTimeStampInMicroseconds() const {
  return output_timestamp_base_in_microseconds_ +
         base::Time::kMicrosecondsPerSecond *
         total_samples_generated_  / samples_per_second_;
}

int ClearKeyCdm::GenerateFakeAudioFramesFromDuration(
    int64 duration_in_microseconds,
    cdm::AudioFrames* audio_frames) const {
  int64 samples_to_generate = static_cast<double>(samples_per_second_) *
      duration_in_microseconds / base::Time::kMicrosecondsPerSecond + 0.5;
  if (samples_to_generate <= 0)
    return 0;

  int64 bytes_per_sample = channel_count_ * bits_per_channel_ / 8;
  // |frame_size| must be a multiple of |bytes_per_sample|.
  int64 frame_size = bytes_per_sample * samples_to_generate;

  int64 timestamp = CurrentTimeStampInMicroseconds();

  const int kHeaderSize = sizeof(timestamp) + sizeof(frame_size);
  audio_frames->SetFrameBuffer(host_->Allocate(kHeaderSize + frame_size));
  uint8_t* data = audio_frames->FrameBuffer()->Data();

  memcpy(data, &timestamp, sizeof(timestamp));
  data += sizeof(timestamp);
  memcpy(data, &frame_size, sizeof(frame_size));
  data += sizeof(frame_size);
  // You won't hear anything because we have all zeros here. But the video
  // should play just fine!
  memset(data, 0, frame_size);

  audio_frames->FrameBuffer()->SetSize(kHeaderSize + frame_size);

  return samples_to_generate;
}

cdm::Status ClearKeyCdm::GenerateFakeAudioFrames(
    int64 timestamp_in_microseconds,
    cdm::AudioFrames* audio_frames) {
  if (timestamp_in_microseconds == kNoTimestamp)
    return cdm::kNeedMoreData;

  // Return kNeedMoreData for the first frame because duration is unknown.
  if (output_timestamp_base_in_microseconds_ == kNoTimestamp) {
    output_timestamp_base_in_microseconds_ = timestamp_in_microseconds;
    return cdm::kNeedMoreData;
  }

  int samples_generated = GenerateFakeAudioFramesFromDuration(
      timestamp_in_microseconds - CurrentTimeStampInMicroseconds(),
      audio_frames);
  total_samples_generated_ += samples_generated;

  return samples_generated == 0 ? cdm::kNeedMoreData : cdm::kSuccess;
}
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER

void ClearKeyCdm::StartFileIOTest() {
  file_io_test_runner_.reset(new FileIOTestRunner(
      base::Bind(&ClearKeyCdmHost::CreateFileIO, base::Unretained(host_))));
  file_io_test_runner_->RunAllTests(
      base::Bind(&ClearKeyCdm::OnFileIOTestComplete, base::Unretained(this)));
}

void ClearKeyCdm::OnFileIOTestComplete(bool success) {
  DVLOG(1) << __FUNCTION__ << ": " << success;
  std::string message = GetFileIOTestResultMessage(success);
  host_->OnSessionMessage(
      last_session_id_, message.data(), message.size(), NULL, 0);
  file_io_test_runner_.reset();
}

}  // namespace media
