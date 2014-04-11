// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/ppapi/cdm_adapter.h"

#include "media/cdm/ppapi/cdm_file_io_impl.h"
#include "media/cdm/ppapi/cdm_helpers.h"
#include "media/cdm/ppapi/cdm_logging.h"
#include "media/cdm/ppapi/supported_cdm_versions.h"
#include "ppapi/c/ppb_console.h"

#if defined(CHECK_DOCUMENT_URL)
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/instance_handle.h"
#endif  // defined(CHECK_DOCUMENT_URL)

namespace {

#if !defined(NDEBUG)
  #define DLOG_TO_CONSOLE(message) LogToConsole(message);
#else
  #define DLOG_TO_CONSOLE(message) (void)(message);
#endif

bool IsMainThread() {
  return pp::Module::Get()->core()->IsMainThread();
}

// Posts a task to run |cb| on the main thread. The task is posted even if the
// current thread is the main thread.
void PostOnMain(pp::CompletionCallback cb) {
  pp::Module::Get()->core()->CallOnMainThread(0, cb, PP_OK);
}

// Ensures |cb| is called on the main thread, either because the current thread
// is the main thread or by posting it to the main thread.
void CallOnMain(pp::CompletionCallback cb) {
  // TODO(tomfinegan): This is only necessary because PPAPI doesn't allow calls
  // off the main thread yet. Remove this once the change lands.
  if (IsMainThread())
    cb.Run(PP_OK);
  else
    PostOnMain(cb);
}

// Configures a cdm::InputBuffer. |subsamples| must exist as long as
// |input_buffer| is in use.
void ConfigureInputBuffer(
    const pp::Buffer_Dev& encrypted_buffer,
    const PP_EncryptedBlockInfo& encrypted_block_info,
    std::vector<cdm::SubsampleEntry>* subsamples,
    cdm::InputBuffer* input_buffer) {
  PP_DCHECK(subsamples);
  PP_DCHECK(!encrypted_buffer.is_null());

  input_buffer->data = static_cast<uint8_t*>(encrypted_buffer.data());
  input_buffer->data_size = encrypted_block_info.data_size;
  PP_DCHECK(encrypted_buffer.size() >= input_buffer->data_size);

  PP_DCHECK(encrypted_block_info.key_id_size <=
            arraysize(encrypted_block_info.key_id));
  input_buffer->key_id_size = encrypted_block_info.key_id_size;
  input_buffer->key_id = input_buffer->key_id_size > 0 ?
      encrypted_block_info.key_id : NULL;

  PP_DCHECK(encrypted_block_info.iv_size <= arraysize(encrypted_block_info.iv));
  input_buffer->iv_size = encrypted_block_info.iv_size;
  input_buffer->iv = encrypted_block_info.iv_size > 0 ?
      encrypted_block_info.iv : NULL;

  input_buffer->num_subsamples = encrypted_block_info.num_subsamples;
  if (encrypted_block_info.num_subsamples > 0) {
    subsamples->reserve(encrypted_block_info.num_subsamples);

    for (uint32_t i = 0; i < encrypted_block_info.num_subsamples; ++i) {
      subsamples->push_back(cdm::SubsampleEntry(
          encrypted_block_info.subsamples[i].clear_bytes,
          encrypted_block_info.subsamples[i].cipher_bytes));
    }

    input_buffer->subsamples = &(*subsamples)[0];
  }

  input_buffer->timestamp = encrypted_block_info.tracking_info.timestamp;
}

PP_DecryptResult CdmStatusToPpDecryptResult(cdm::Status status) {
  switch (status) {
    case cdm::kSuccess:
      return PP_DECRYPTRESULT_SUCCESS;
    case cdm::kNoKey:
      return PP_DECRYPTRESULT_DECRYPT_NOKEY;
    case cdm::kNeedMoreData:
      return PP_DECRYPTRESULT_NEEDMOREDATA;
    case cdm::kDecryptError:
      return PP_DECRYPTRESULT_DECRYPT_ERROR;
    case cdm::kDecodeError:
      return PP_DECRYPTRESULT_DECODE_ERROR;
    default:
      PP_NOTREACHED();
      return PP_DECRYPTRESULT_DECODE_ERROR;
  }
}

PP_DecryptedFrameFormat CdmVideoFormatToPpDecryptedFrameFormat(
    cdm::VideoFormat format) {
  switch (format) {
    case cdm::kYv12:
      return PP_DECRYPTEDFRAMEFORMAT_YV12;
    case cdm::kI420:
      return PP_DECRYPTEDFRAMEFORMAT_I420;
    default:
      return PP_DECRYPTEDFRAMEFORMAT_UNKNOWN;
  }
}

PP_DecryptedSampleFormat CdmAudioFormatToPpDecryptedSampleFormat(
    cdm::AudioFormat format) {
  switch (format) {
    case cdm::kAudioFormatU8:
      return PP_DECRYPTEDSAMPLEFORMAT_U8;
    case cdm::kAudioFormatS16:
      return PP_DECRYPTEDSAMPLEFORMAT_S16;
    case cdm::kAudioFormatS32:
      return PP_DECRYPTEDSAMPLEFORMAT_S32;
    case cdm::kAudioFormatF32:
      return PP_DECRYPTEDSAMPLEFORMAT_F32;
    case cdm::kAudioFormatPlanarS16:
      return PP_DECRYPTEDSAMPLEFORMAT_PLANAR_S16;
    case cdm::kAudioFormatPlanarF32:
      return PP_DECRYPTEDSAMPLEFORMAT_PLANAR_F32;
    default:
      return PP_DECRYPTEDSAMPLEFORMAT_UNKNOWN;
  }
}

cdm::AudioDecoderConfig::AudioCodec PpAudioCodecToCdmAudioCodec(
    PP_AudioCodec codec) {
  switch (codec) {
    case PP_AUDIOCODEC_VORBIS:
      return cdm::AudioDecoderConfig::kCodecVorbis;
    case PP_AUDIOCODEC_AAC:
      return cdm::AudioDecoderConfig::kCodecAac;
    default:
      return cdm::AudioDecoderConfig::kUnknownAudioCodec;
  }
}

cdm::VideoDecoderConfig::VideoCodec PpVideoCodecToCdmVideoCodec(
    PP_VideoCodec codec) {
  switch (codec) {
    case PP_VIDEOCODEC_VP8:
      return cdm::VideoDecoderConfig::kCodecVp8;
    case PP_VIDEOCODEC_H264:
      return cdm::VideoDecoderConfig::kCodecH264;
    case PP_VIDEOCODEC_VP9:
      return cdm::VideoDecoderConfig::kCodecVp9;
    default:
      return cdm::VideoDecoderConfig::kUnknownVideoCodec;
  }
}

cdm::VideoDecoderConfig::VideoCodecProfile PpVCProfileToCdmVCProfile(
    PP_VideoCodecProfile profile) {
  switch (profile) {
    case PP_VIDEOCODECPROFILE_NOT_NEEDED:
      return cdm::VideoDecoderConfig::kProfileNotNeeded;
    case PP_VIDEOCODECPROFILE_H264_BASELINE:
      return cdm::VideoDecoderConfig::kH264ProfileBaseline;
    case PP_VIDEOCODECPROFILE_H264_MAIN:
      return cdm::VideoDecoderConfig::kH264ProfileMain;
    case PP_VIDEOCODECPROFILE_H264_EXTENDED:
      return cdm::VideoDecoderConfig::kH264ProfileExtended;
    case PP_VIDEOCODECPROFILE_H264_HIGH:
      return cdm::VideoDecoderConfig::kH264ProfileHigh;
    case PP_VIDEOCODECPROFILE_H264_HIGH_10:
      return cdm::VideoDecoderConfig::kH264ProfileHigh10;
    case PP_VIDEOCODECPROFILE_H264_HIGH_422:
      return cdm::VideoDecoderConfig::kH264ProfileHigh422;
    case PP_VIDEOCODECPROFILE_H264_HIGH_444_PREDICTIVE:
      return cdm::VideoDecoderConfig::kH264ProfileHigh444Predictive;
    default:
      return cdm::VideoDecoderConfig::kUnknownVideoCodecProfile;
  }
}

cdm::VideoFormat PpDecryptedFrameFormatToCdmVideoFormat(
    PP_DecryptedFrameFormat format) {
  switch (format) {
    case PP_DECRYPTEDFRAMEFORMAT_YV12:
      return cdm::kYv12;
    case PP_DECRYPTEDFRAMEFORMAT_I420:
      return cdm::kI420;
    default:
      return cdm::kUnknownVideoFormat;
  }
}

cdm::StreamType PpDecryptorStreamTypeToCdmStreamType(
    PP_DecryptorStreamType stream_type) {
  switch (stream_type) {
    case PP_DECRYPTORSTREAMTYPE_AUDIO:
      return cdm::kStreamTypeAudio;
    case PP_DECRYPTORSTREAMTYPE_VIDEO:
      return cdm::kStreamTypeVideo;
  }

  PP_NOTREACHED();
  return cdm::kStreamTypeVideo;
}

}  // namespace

namespace media {

CdmAdapter::CdmAdapter(PP_Instance instance, pp::Module* module)
    : pp::Instance(instance),
      pp::ContentDecryptor_Private(this),
#if defined(OS_CHROMEOS)
      output_protection_(this),
      platform_verification_(this),
      challenge_in_progress_(false),
      output_link_mask_(0),
      output_protection_mask_(0),
      query_output_protection_in_progress_(false),
#endif
      allocator_(this),
      cdm_(NULL),
      deferred_initialize_audio_decoder_(false),
      deferred_audio_decoder_config_id_(0),
      deferred_initialize_video_decoder_(false),
      deferred_video_decoder_config_id_(0) {
  callback_factory_.Initialize(this);
}

CdmAdapter::~CdmAdapter() {}

bool CdmAdapter::CreateCdmInstance(const std::string& key_system) {
  PP_DCHECK(!cdm_);
  cdm_ = make_linked_ptr(CdmWrapper::Create(
      key_system.data(), key_system.size(), GetCdmHost, this));
  bool success = cdm_ != NULL;

  const std::string message = "CDM instance for " + key_system +
                              (success ? "" : " could not be") + " created.";
  DLOG_TO_CONSOLE(message);
  CDM_DLOG() << message;

  return success;
}

// No errors should be reported in this function because the spec says:
// "Store this new error object internally with the MediaKeys instance being
// created. This will be used to fire an error against any session created for
// this instance." These errors will be reported during session creation
// (CreateSession()) or session loading (LoadSession()).
// TODO(xhwang): If necessary, we need to store the error here if we want to
// support more specific error reporting (other than "Unknown").
void CdmAdapter::Initialize(const std::string& key_system) {
  PP_DCHECK(!key_system.empty());
  PP_DCHECK(key_system_.empty() || (key_system_ == key_system && cdm_));

  if (!cdm_ && !CreateCdmInstance(key_system))
    return;

  PP_DCHECK(cdm_);
  key_system_ = key_system;
}

void CdmAdapter::CreateSession(uint32_t session_id,
                               const std::string& content_type,
                               pp::VarArrayBuffer init_data) {
  // Initialize() doesn't report an error, so CreateSession() can be called
  // even if Initialize() failed.
  if (!cdm_) {
    OnSessionError(session_id, cdm::kUnknownError, 0);
    return;
  }

#if defined(CHECK_DOCUMENT_URL)
  PP_URLComponents_Dev url_components = {};
  const pp::URLUtil_Dev* url_util = pp::URLUtil_Dev::Get();
  if (!url_util) {
    OnSessionError(session_id, cdm::kUnknownError, 0);
    return;
  }
  pp::Var href = url_util->GetDocumentURL(
      pp::InstanceHandle(pp_instance()), &url_components);
  PP_DCHECK(href.is_string());
  PP_DCHECK(!href.AsString().empty());
  PP_DCHECK(url_components.host.begin);
  PP_DCHECK(0 < url_components.host.len);
#endif  // defined(CHECK_DOCUMENT_URL)

  cdm_->CreateSession(session_id,
                      content_type.data(),
                      content_type.size(),
                      static_cast<const uint8_t*>(init_data.Map()),
                      init_data.ByteLength());
}

void CdmAdapter::LoadSession(uint32_t session_id,
                             const std::string& web_session_id) {
  // Initialize() doesn't report an error, so LoadSession() can be called
  // even if Initialize() failed.
  if (!cdm_) {
    OnSessionError(session_id, cdm::kUnknownError, 0);
    return;
  }

  cdm_->LoadSession(session_id, web_session_id.data(), web_session_id.size());
}

void CdmAdapter::UpdateSession(uint32_t session_id,
                               pp::VarArrayBuffer response) {
  const uint8_t* response_ptr = static_cast<const uint8_t*>(response.Map());
  const uint32_t response_size = response.ByteLength();

  if (!response_ptr || response_size <= 0) {
    OnSessionError(session_id, cdm::kUnknownError, 0);
    return;
  }
  cdm_->UpdateSession(session_id, response_ptr, response_size);
}

void CdmAdapter::ReleaseSession(uint32_t session_id) {
  cdm_->ReleaseSession(session_id);
}

// Note: In the following decryption/decoding related functions, errors are NOT
// reported via KeyError, but are reported via corresponding PPB calls.

void CdmAdapter::Decrypt(pp::Buffer_Dev encrypted_buffer,
                         const PP_EncryptedBlockInfo& encrypted_block_info) {
  PP_DCHECK(!encrypted_buffer.is_null());

  // Release a buffer that the caller indicated it is finished with.
  allocator_.Release(encrypted_block_info.tracking_info.buffer_id);

  cdm::Status status = cdm::kDecryptError;
  LinkedDecryptedBlock decrypted_block(new DecryptedBlockImpl());

  if (cdm_) {
    cdm::InputBuffer input_buffer;
    std::vector<cdm::SubsampleEntry> subsamples;
    ConfigureInputBuffer(encrypted_buffer, encrypted_block_info, &subsamples,
                         &input_buffer);
    status = cdm_->Decrypt(input_buffer, decrypted_block.get());
    PP_DCHECK(status != cdm::kSuccess ||
              (decrypted_block->DecryptedBuffer() &&
               decrypted_block->DecryptedBuffer()->Size()));
  }

  CallOnMain(callback_factory_.NewCallback(
      &CdmAdapter::DeliverBlock,
      status,
      decrypted_block,
      encrypted_block_info.tracking_info));
}

void CdmAdapter::InitializeAudioDecoder(
    const PP_AudioDecoderConfig& decoder_config,
    pp::Buffer_Dev extra_data_buffer) {
  PP_DCHECK(!deferred_initialize_audio_decoder_);
  PP_DCHECK(deferred_audio_decoder_config_id_ == 0);
  cdm::Status status = cdm::kSessionError;
  if (cdm_) {
    cdm::AudioDecoderConfig cdm_decoder_config;
    cdm_decoder_config.codec =
        PpAudioCodecToCdmAudioCodec(decoder_config.codec);
    cdm_decoder_config.channel_count = decoder_config.channel_count;
    cdm_decoder_config.bits_per_channel = decoder_config.bits_per_channel;
    cdm_decoder_config.samples_per_second = decoder_config.samples_per_second;
    cdm_decoder_config.extra_data =
        static_cast<uint8_t*>(extra_data_buffer.data());
    cdm_decoder_config.extra_data_size = extra_data_buffer.size();
    status = cdm_->InitializeAudioDecoder(cdm_decoder_config);
  }

  if (status == cdm::kDeferredInitialization) {
    deferred_initialize_audio_decoder_ = true;
    deferred_audio_decoder_config_id_ = decoder_config.request_id;
    return;
  }

  CallOnMain(callback_factory_.NewCallback(
      &CdmAdapter::DecoderInitializeDone,
      PP_DECRYPTORSTREAMTYPE_AUDIO,
      decoder_config.request_id,
      status == cdm::kSuccess));
}

void CdmAdapter::InitializeVideoDecoder(
    const PP_VideoDecoderConfig& decoder_config,
    pp::Buffer_Dev extra_data_buffer) {
  PP_DCHECK(!deferred_initialize_video_decoder_);
  PP_DCHECK(deferred_video_decoder_config_id_ == 0);
  cdm::Status status = cdm::kSessionError;
  if (cdm_) {
    cdm::VideoDecoderConfig cdm_decoder_config;
    cdm_decoder_config.codec =
        PpVideoCodecToCdmVideoCodec(decoder_config.codec);
    cdm_decoder_config.profile =
        PpVCProfileToCdmVCProfile(decoder_config.profile);
    cdm_decoder_config.format =
        PpDecryptedFrameFormatToCdmVideoFormat(decoder_config.format);
    cdm_decoder_config.coded_size.width = decoder_config.width;
    cdm_decoder_config.coded_size.height = decoder_config.height;
    cdm_decoder_config.extra_data =
        static_cast<uint8_t*>(extra_data_buffer.data());
    cdm_decoder_config.extra_data_size = extra_data_buffer.size();
    status = cdm_->InitializeVideoDecoder(cdm_decoder_config);
  }

  if (status == cdm::kDeferredInitialization) {
    deferred_initialize_video_decoder_ = true;
    deferred_video_decoder_config_id_ = decoder_config.request_id;
    return;
  }

  CallOnMain(callback_factory_.NewCallback(
      &CdmAdapter::DecoderInitializeDone,
      PP_DECRYPTORSTREAMTYPE_VIDEO,
      decoder_config.request_id,
      status == cdm::kSuccess));
}

void CdmAdapter::DeinitializeDecoder(PP_DecryptorStreamType decoder_type,
                                     uint32_t request_id) {
  PP_DCHECK(cdm_);  // InitializeXxxxxDecoder should have succeeded.
  if (cdm_) {
    cdm_->DeinitializeDecoder(
        PpDecryptorStreamTypeToCdmStreamType(decoder_type));
  }

  CallOnMain(callback_factory_.NewCallback(
      &CdmAdapter::DecoderDeinitializeDone,
      decoder_type,
      request_id));
}

void CdmAdapter::ResetDecoder(PP_DecryptorStreamType decoder_type,
                              uint32_t request_id) {
  PP_DCHECK(cdm_);  // InitializeXxxxxDecoder should have succeeded.
  if (cdm_)
    cdm_->ResetDecoder(PpDecryptorStreamTypeToCdmStreamType(decoder_type));

  CallOnMain(callback_factory_.NewCallback(&CdmAdapter::DecoderResetDone,
                                           decoder_type,
                                           request_id));
}

void CdmAdapter::DecryptAndDecode(
    PP_DecryptorStreamType decoder_type,
    pp::Buffer_Dev encrypted_buffer,
    const PP_EncryptedBlockInfo& encrypted_block_info) {
  PP_DCHECK(cdm_);  // InitializeXxxxxDecoder should have succeeded.
  // Release a buffer that the caller indicated it is finished with.
  allocator_.Release(encrypted_block_info.tracking_info.buffer_id);

  cdm::InputBuffer input_buffer;
  std::vector<cdm::SubsampleEntry> subsamples;
  if (cdm_ && !encrypted_buffer.is_null()) {
    ConfigureInputBuffer(encrypted_buffer,
                         encrypted_block_info,
                         &subsamples,
                         &input_buffer);
  }

  cdm::Status status = cdm::kDecodeError;

  switch (decoder_type) {
    case PP_DECRYPTORSTREAMTYPE_VIDEO: {
      LinkedVideoFrame video_frame(new VideoFrameImpl());
      if (cdm_)
        status = cdm_->DecryptAndDecodeFrame(input_buffer, video_frame.get());
      CallOnMain(callback_factory_.NewCallback(
          &CdmAdapter::DeliverFrame,
          status,
          video_frame,
          encrypted_block_info.tracking_info));
      return;
    }

    case PP_DECRYPTORSTREAMTYPE_AUDIO: {
      LinkedAudioFrames audio_frames(new AudioFramesImpl());
      if (cdm_) {
        status = cdm_->DecryptAndDecodeSamples(input_buffer,
                                               audio_frames.get());
      }
      CallOnMain(callback_factory_.NewCallback(
          &CdmAdapter::DeliverSamples,
          status,
          audio_frames,
          encrypted_block_info.tracking_info));
      return;
    }

    default:
      PP_NOTREACHED();
      return;
  }
}

cdm::Buffer* CdmAdapter::Allocate(uint32_t capacity) {
  return allocator_.Allocate(capacity);
}

void CdmAdapter::SetTimer(int64_t delay_ms, void* context) {
  // NOTE: doesn't really need to run on the main thread; could just as well run
  // on a helper thread if |cdm_| were thread-friendly and care was taken.  We
  // only use CallOnMainThread() here to get delayed-execution behavior.
  pp::Module::Get()->core()->CallOnMainThread(
      delay_ms,
      callback_factory_.NewCallback(&CdmAdapter::TimerExpired, context),
      PP_OK);
}

void CdmAdapter::TimerExpired(int32_t result, void* context) {
  PP_DCHECK(result == PP_OK);
  cdm_->TimerExpired(context);
}

double CdmAdapter::GetCurrentWallTimeInSeconds() {
  return pp::Module::Get()->core()->GetTime();
}

void CdmAdapter::OnSessionCreated(uint32_t session_id,
                                  const char* web_session_id,
                                  uint32_t web_session_id_length) {
  PostOnMain(callback_factory_.NewCallback(
      &CdmAdapter::SendSessionCreatedInternal,
      session_id,
      std::string(web_session_id, web_session_id_length)));
}

void CdmAdapter::OnSessionMessage(uint32_t session_id,
                                  const char* message,
                                  uint32_t message_length,
                                  const char* destination_url,
                                  uint32_t destination_url_length) {
  PostOnMain(callback_factory_.NewCallback(
      &CdmAdapter::SendSessionMessageInternal,
      session_id,
      std::vector<uint8>(message, message + message_length),
      std::string(destination_url, destination_url_length)));
}

void CdmAdapter::OnSessionReady(uint32_t session_id) {
  PostOnMain(callback_factory_.NewCallback(
      &CdmAdapter::SendSessionReadyInternal, session_id));
}

void CdmAdapter::OnSessionClosed(uint32_t session_id) {
  PostOnMain(callback_factory_.NewCallback(
      &CdmAdapter::SendSessionClosedInternal, session_id));
}

void CdmAdapter::OnSessionError(uint32_t session_id,
                                cdm::MediaKeyError error_code,
                                uint32_t system_code) {
  PostOnMain(callback_factory_.NewCallback(
      &CdmAdapter::SendSessionErrorInternal,
      session_id,
      error_code,
      system_code));
}

void CdmAdapter::SendSessionCreatedInternal(int32_t result,
                                            uint32_t session_id,
                                            const std::string& web_session_id) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::SessionCreated(session_id, web_session_id);
}

void CdmAdapter::SendSessionMessageInternal(int32_t result,
                                            uint32_t session_id,
                                            const std::vector<uint8>& message,
                                            const std::string& default_url) {
  PP_DCHECK(result == PP_OK);

  pp::VarArrayBuffer message_array_buffer(message.size());
  if (message.size() > 0) {
    memcpy(message_array_buffer.Map(), message.data(), message.size());
  }

  pp::ContentDecryptor_Private::SessionMessage(
      session_id, message_array_buffer, default_url);
}

void CdmAdapter::SendSessionReadyInternal(int32_t result, uint32_t session_id) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::SessionReady(session_id);
}

void CdmAdapter::SendSessionClosedInternal(int32_t result,
                                           uint32_t session_id) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::SessionClosed(session_id);
}

void CdmAdapter::SendSessionErrorInternal(int32_t result,
                                          uint32_t session_id,
                                          cdm::MediaKeyError error_code,
                                          uint32_t system_code) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::SessionError(
      session_id, error_code, system_code);
}

void CdmAdapter::DeliverBlock(int32_t result,
                              const cdm::Status& status,
                              const LinkedDecryptedBlock& decrypted_block,
                              const PP_DecryptTrackingInfo& tracking_info) {
  PP_DCHECK(result == PP_OK);
  PP_DecryptedBlockInfo decrypted_block_info;
  decrypted_block_info.tracking_info = tracking_info;
  decrypted_block_info.tracking_info.timestamp = decrypted_block->Timestamp();
  decrypted_block_info.tracking_info.buffer_id = 0;
  decrypted_block_info.data_size = 0;
  decrypted_block_info.result = CdmStatusToPpDecryptResult(status);

  pp::Buffer_Dev buffer;

  if (decrypted_block_info.result == PP_DECRYPTRESULT_SUCCESS) {
    PP_DCHECK(decrypted_block.get() && decrypted_block->DecryptedBuffer());
    if (!decrypted_block.get() || !decrypted_block->DecryptedBuffer()) {
      PP_NOTREACHED();
      decrypted_block_info.result = PP_DECRYPTRESULT_DECRYPT_ERROR;
    } else {
      PpbBuffer* ppb_buffer =
          static_cast<PpbBuffer*>(decrypted_block->DecryptedBuffer());
      buffer = ppb_buffer->buffer_dev();
      decrypted_block_info.tracking_info.buffer_id = ppb_buffer->buffer_id();
      decrypted_block_info.data_size = ppb_buffer->Size();
    }
  }

  pp::ContentDecryptor_Private::DeliverBlock(buffer, decrypted_block_info);
}

void CdmAdapter::DecoderInitializeDone(int32_t result,
                                       PP_DecryptorStreamType decoder_type,
                                       uint32_t request_id,
                                       bool success) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::DecoderInitializeDone(decoder_type,
                                                      request_id,
                                                      success);
}

void CdmAdapter::DecoderDeinitializeDone(int32_t result,
                                         PP_DecryptorStreamType decoder_type,
                                         uint32_t request_id) {
  pp::ContentDecryptor_Private::DecoderDeinitializeDone(decoder_type,
                                                        request_id);
}

void CdmAdapter::DecoderResetDone(int32_t result,
                                  PP_DecryptorStreamType decoder_type,
                                  uint32_t request_id) {
  pp::ContentDecryptor_Private::DecoderResetDone(decoder_type, request_id);
}

void CdmAdapter::DeliverFrame(
    int32_t result,
    const cdm::Status& status,
    const LinkedVideoFrame& video_frame,
    const PP_DecryptTrackingInfo& tracking_info) {
  PP_DCHECK(result == PP_OK);
  PP_DecryptedFrameInfo decrypted_frame_info;
  decrypted_frame_info.tracking_info.request_id = tracking_info.request_id;
  decrypted_frame_info.tracking_info.buffer_id = 0;
  decrypted_frame_info.result = CdmStatusToPpDecryptResult(status);

  pp::Buffer_Dev buffer;

  if (decrypted_frame_info.result == PP_DECRYPTRESULT_SUCCESS) {
    if (!IsValidVideoFrame(video_frame)) {
      PP_NOTREACHED();
      decrypted_frame_info.result = PP_DECRYPTRESULT_DECODE_ERROR;
    } else {
      PpbBuffer* ppb_buffer =
          static_cast<PpbBuffer*>(video_frame->FrameBuffer());

      buffer = ppb_buffer->buffer_dev();

      decrypted_frame_info.tracking_info.timestamp = video_frame->Timestamp();
      decrypted_frame_info.tracking_info.buffer_id = ppb_buffer->buffer_id();
      decrypted_frame_info.format =
          CdmVideoFormatToPpDecryptedFrameFormat(video_frame->Format());
      decrypted_frame_info.width = video_frame->Size().width;
      decrypted_frame_info.height = video_frame->Size().height;
      decrypted_frame_info.plane_offsets[PP_DECRYPTEDFRAMEPLANES_Y] =
          video_frame->PlaneOffset(cdm::VideoFrame::kYPlane);
      decrypted_frame_info.plane_offsets[PP_DECRYPTEDFRAMEPLANES_U] =
          video_frame->PlaneOffset(cdm::VideoFrame::kUPlane);
      decrypted_frame_info.plane_offsets[PP_DECRYPTEDFRAMEPLANES_V] =
          video_frame->PlaneOffset(cdm::VideoFrame::kVPlane);
      decrypted_frame_info.strides[PP_DECRYPTEDFRAMEPLANES_Y] =
          video_frame->Stride(cdm::VideoFrame::kYPlane);
      decrypted_frame_info.strides[PP_DECRYPTEDFRAMEPLANES_U] =
          video_frame->Stride(cdm::VideoFrame::kUPlane);
      decrypted_frame_info.strides[PP_DECRYPTEDFRAMEPLANES_V] =
          video_frame->Stride(cdm::VideoFrame::kVPlane);
    }
  }
  pp::ContentDecryptor_Private::DeliverFrame(buffer, decrypted_frame_info);
}

void CdmAdapter::DeliverSamples(int32_t result,
                                const cdm::Status& status,
                                const LinkedAudioFrames& audio_frames,
                                const PP_DecryptTrackingInfo& tracking_info) {
  PP_DCHECK(result == PP_OK);

  PP_DecryptedSampleInfo decrypted_sample_info;
  decrypted_sample_info.tracking_info = tracking_info;
  decrypted_sample_info.tracking_info.timestamp = 0;
  decrypted_sample_info.tracking_info.buffer_id = 0;
  decrypted_sample_info.data_size = 0;
  decrypted_sample_info.result = CdmStatusToPpDecryptResult(status);

  pp::Buffer_Dev buffer;

  if (decrypted_sample_info.result == PP_DECRYPTRESULT_SUCCESS) {
    PP_DCHECK(audio_frames.get() && audio_frames->FrameBuffer());
    if (!audio_frames.get() || !audio_frames->FrameBuffer()) {
      PP_NOTREACHED();
      decrypted_sample_info.result = PP_DECRYPTRESULT_DECRYPT_ERROR;
    } else {
      PpbBuffer* ppb_buffer =
          static_cast<PpbBuffer*>(audio_frames->FrameBuffer());
      buffer = ppb_buffer->buffer_dev();
      decrypted_sample_info.tracking_info.buffer_id = ppb_buffer->buffer_id();
      decrypted_sample_info.data_size = ppb_buffer->Size();
      decrypted_sample_info.format =
          CdmAudioFormatToPpDecryptedSampleFormat(audio_frames->Format());
    }
  }

  pp::ContentDecryptor_Private::DeliverSamples(buffer, decrypted_sample_info);
}

bool CdmAdapter::IsValidVideoFrame(const LinkedVideoFrame& video_frame) {
  if (!video_frame.get() ||
      !video_frame->FrameBuffer() ||
      (video_frame->Format() != cdm::kI420 &&
       video_frame->Format() != cdm::kYv12)) {
    CDM_DLOG() << "Invalid video frame!";
    return false;
  }

  PpbBuffer* ppb_buffer = static_cast<PpbBuffer*>(video_frame->FrameBuffer());

  for (uint32_t i = 0; i < cdm::VideoFrame::kMaxPlanes; ++i) {
    int plane_height = (i == cdm::VideoFrame::kYPlane) ?
        video_frame->Size().height : (video_frame->Size().height + 1) / 2;
    cdm::VideoFrame::VideoPlane plane =
        static_cast<cdm::VideoFrame::VideoPlane>(i);
    if (ppb_buffer->Size() < video_frame->PlaneOffset(plane) +
                             plane_height * video_frame->Stride(plane)) {
      return false;
    }
  }

  return true;
}

#if !defined(NDEBUG)
void CdmAdapter::LogToConsole(const pp::Var& value) {
  PP_DCHECK(IsMainThread());
  const PPB_Console* console = reinterpret_cast<const PPB_Console*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CONSOLE_INTERFACE));
  console->Log(pp_instance(), PP_LOGLEVEL_LOG, value.pp_var());
}
#endif  // !defined(NDEBUG)

void CdmAdapter::SendPlatformChallenge(
    const char* service_id, uint32_t service_id_length,
    const char* challenge, uint32_t challenge_length) {
#if defined(OS_CHROMEOS)
  PP_DCHECK(!challenge_in_progress_);

  // Ensure member variables set by the callback are in a clean state.
  signed_data_output_ = pp::Var();
  signed_data_signature_output_ = pp::Var();
  platform_key_certificate_output_ = pp::Var();

  pp::VarArrayBuffer challenge_var(challenge_length);
  uint8_t* var_data = static_cast<uint8_t*>(challenge_var.Map());
  memcpy(var_data, challenge, challenge_length);

  std::string service_id_str(service_id, service_id_length);
  int32_t result = platform_verification_.ChallengePlatform(
      pp::Var(service_id_str), challenge_var, &signed_data_output_,
      &signed_data_signature_output_, &platform_key_certificate_output_,
      callback_factory_.NewCallback(&CdmAdapter::SendPlatformChallengeDone));
  challenge_var.Unmap();
  if (result == PP_OK_COMPLETIONPENDING) {
    challenge_in_progress_ = true;
    return;
  }

  // Fall through on error and issue an empty OnPlatformChallengeResponse().
  PP_DCHECK(result != PP_OK);
#endif

  cdm::PlatformChallengeResponse response = {};
  cdm_->OnPlatformChallengeResponse(response);
}

void CdmAdapter::EnableOutputProtection(uint32_t desired_protection_mask) {
#if defined(OS_CHROMEOS)
  int32_t result = output_protection_.EnableProtection(
      desired_protection_mask, callback_factory_.NewCallback(
          &CdmAdapter::EnableProtectionDone));

  // Errors are ignored since clients must call QueryOutputProtectionStatus() to
  // inspect the protection status on a regular basis.

  if (result != PP_OK && result != PP_OK_COMPLETIONPENDING)
    CDM_DLOG() << __FUNCTION__ << " failed!";
#endif
}

void CdmAdapter::QueryOutputProtectionStatus() {
#if defined(OS_CHROMEOS)
  PP_DCHECK(!query_output_protection_in_progress_);

  output_link_mask_ = output_protection_mask_ = 0;
  const int32_t result = output_protection_.QueryStatus(
      &output_link_mask_,
      &output_protection_mask_,
      callback_factory_.NewCallback(
          &CdmAdapter::QueryOutputProtectionStatusDone));
  if (result == PP_OK_COMPLETIONPENDING) {
    query_output_protection_in_progress_ = true;
    return;
  }

  // Fall through on error and issue an empty OnQueryOutputProtectionStatus().
  PP_DCHECK(result != PP_OK);
#endif

  cdm_->OnQueryOutputProtectionStatus(0, 0);
}

void CdmAdapter::OnDeferredInitializationDone(cdm::StreamType stream_type,
                                              cdm::Status decoder_status) {
  switch (stream_type) {
    case cdm::kStreamTypeAudio:
      PP_DCHECK(deferred_initialize_audio_decoder_);
      CallOnMain(
          callback_factory_.NewCallback(&CdmAdapter::DecoderInitializeDone,
                                        PP_DECRYPTORSTREAMTYPE_AUDIO,
                                        deferred_audio_decoder_config_id_,
                                        decoder_status == cdm::kSuccess));
      deferred_initialize_audio_decoder_ = false;
      deferred_audio_decoder_config_id_ = 0;
      break;
    case cdm::kStreamTypeVideo:
      PP_DCHECK(deferred_initialize_video_decoder_);
      CallOnMain(
          callback_factory_.NewCallback(&CdmAdapter::DecoderInitializeDone,
                                        PP_DECRYPTORSTREAMTYPE_VIDEO,
                                        deferred_video_decoder_config_id_,
                                        decoder_status == cdm::kSuccess));
      deferred_initialize_video_decoder_ = false;
      deferred_video_decoder_config_id_ = 0;
      break;
  }
}

// The CDM owns the returned object and must call FileIO::Close() to release it.
cdm::FileIO* CdmAdapter::CreateFileIO(cdm::FileIOClient* client) {
  return new CdmFileIOImpl(client, pp_instance());
}

#if defined(OS_CHROMEOS)
void CdmAdapter::SendPlatformChallengeDone(int32_t result) {
  challenge_in_progress_ = false;

  if (result != PP_OK) {
    CDM_DLOG() << __FUNCTION__ << ": Platform challenge failed!";
    cdm::PlatformChallengeResponse response = {};
    cdm_->OnPlatformChallengeResponse(response);
    return;
  }

  pp::VarArrayBuffer signed_data_var(signed_data_output_);
  pp::VarArrayBuffer signed_data_signature_var(signed_data_signature_output_);
  std::string platform_key_certificate_string =
      platform_key_certificate_output_.AsString();

  cdm::PlatformChallengeResponse response = {
    static_cast<uint8_t*>(signed_data_var.Map()),
    signed_data_var.ByteLength(),

    static_cast<uint8_t*>(signed_data_signature_var.Map()),
    signed_data_signature_var.ByteLength(),

    reinterpret_cast<const uint8_t*>(platform_key_certificate_string.c_str()),
    static_cast<uint32_t>(platform_key_certificate_string.length())
  };
  cdm_->OnPlatformChallengeResponse(response);

  signed_data_var.Unmap();
  signed_data_signature_var.Unmap();
}

void CdmAdapter::EnableProtectionDone(int32_t result) {
  // Does nothing since clients must call QueryOutputProtectionStatus() to
  // inspect the protection status on a regular basis.
  CDM_DLOG() << __FUNCTION__ << " : " << result;
}

void CdmAdapter::QueryOutputProtectionStatusDone(int32_t result) {
  PP_DCHECK(query_output_protection_in_progress_);
  query_output_protection_in_progress_ = false;

  // Return a protection status of none on error.
  if (result != PP_OK)
    output_link_mask_ = output_protection_mask_ = 0;

  cdm_->OnQueryOutputProtectionStatus(output_link_mask_,
                                      output_protection_mask_);
}
#endif

void* GetCdmHost(int host_interface_version, void* user_data) {
  if (!host_interface_version || !user_data)
    return NULL;

  COMPILE_ASSERT(
      cdm::ContentDecryptionModule::Host::kVersion == cdm::Host_4::kVersion,
      update_code_below);

  // Ensure IsSupportedCdmHostVersion matches implementation of this function.
  // Always update this DCHECK when updating this function.
  // If this check fails, update this function and DCHECK or update
  // IsSupportedCdmHostVersion.

  PP_DCHECK(
      // Future version is not supported.
      !IsSupportedCdmHostVersion(cdm::Host_4::kVersion + 1) &&
      // Current version is supported.
      IsSupportedCdmHostVersion(cdm::Host_4::kVersion) &&
      // Include all previous supported versions (if any) here.
      // No supported previous versions.
      // One older than the oldest supported version is not supported.
      !IsSupportedCdmHostVersion(cdm::Host_4::kVersion - 1));
  PP_DCHECK(IsSupportedCdmHostVersion(host_interface_version));

  CdmAdapter* cdm_adapter = static_cast<CdmAdapter*>(user_data);
  CDM_DLOG() << "Create CDM Host with version " << host_interface_version;
  switch (host_interface_version) {
    case cdm::Host_4::kVersion:
      return static_cast<cdm::Host_4*>(cdm_adapter);
    default:
      PP_NOTREACHED();
      return NULL;
  }
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class CdmAdapterModule : public pp::Module {
 public:
  CdmAdapterModule() : pp::Module() {
    // This function blocks the renderer thread (PluginInstance::Initialize()).
    // Move this call to other places if this may be a concern in the future.
    INITIALIZE_CDM_MODULE();
  }
  virtual ~CdmAdapterModule() {
    DeinitializeCdmModule();
  }

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new CdmAdapter(instance, this);
  }

 private:
  CdmFileIOImpl::ResourceTracker cdm_file_io_impl_resource_tracker;
};

}  // namespace media

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new media::CdmAdapterModule();
}

}  // namespace pp
