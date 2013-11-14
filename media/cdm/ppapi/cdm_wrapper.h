// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_CDM_WRAPPER_H_
#define MEDIA_CDM_PPAPI_CDM_WRAPPER_H_

#include <map>
#include <queue>
#include <string>

#include "base/basictypes.h"
#include "media/cdm/ppapi/api/content_decryption_module.h"
#include "media/cdm/ppapi/cdm_helpers.h"
#include "media/cdm/ppapi/supported_cdm_versions.h"
#include "ppapi/cpp/logging.h"

namespace media {

// CdmWrapper wraps different versions of ContentDecryptionModule interfaces and
// exposes a common interface to the caller.
//
// The caller should call CdmWrapper::Create() to create a CDM instance.
// CdmWrapper will first try to create a CDM instance that supports the latest
// CDM interface (ContentDecryptionModule). If such an instance cannot be
// created (e.g. an older CDM was loaded), CdmWrapper will try to create a CDM
// that supports an older version of CDM interface (e.g.
// ContentDecryptionModule_*). Internally CdmWrapper converts the CdmWrapper
// calls to corresponding ContentDecryptionModule calls.
//
// Note that CdmWrapper interface always reflects the latest state of content
// decryption related PPAPI APIs (e.g. pp::ContentDecryptor_Private).
//
// Since this file is highly templated and default implementations are short
// (just a shim layer in most cases), everything is done in this header file.
class CdmWrapper {
 public:
  // CDM_1 and CDM_2 methods AddKey() and CancelKeyRequest() may require
  // callbacks to fire. Use this enum to indicate the additional calls required.
  // TODO(jrummell): Remove return value once CDM_1 and CDM_2 are no longer
  // supported.
  enum Result {
    NO_ACTION,
    CALL_KEY_ADDED,
    CALL_KEY_ERROR
  };

  static CdmWrapper* Create(const char* key_system,
                            uint32_t key_system_size,
                            GetCdmHostFunc get_cdm_host_func,
                            void* user_data);

  virtual ~CdmWrapper() {};

  virtual void GenerateKeyRequest(uint32_t reference_id,
                                  const char* type,
                                  uint32_t type_size,
                                  const uint8_t* init_data,
                                  uint32_t init_data_size) = 0;
  virtual Result AddKey(uint32_t reference_id,
                        const uint8_t* key,
                        uint32_t key_size,
                        const uint8_t* key_id,
                        uint32_t key_id_size) = 0;
  virtual Result CancelKeyRequest(uint32_t reference_id) = 0;
  virtual void TimerExpired(void* context) = 0;
  virtual cdm::Status Decrypt(const cdm::InputBuffer& encrypted_buffer,
                              cdm::DecryptedBlock* decrypted_buffer) = 0;
  virtual cdm::Status InitializeAudioDecoder(
      const cdm::AudioDecoderConfig& audio_decoder_config) = 0;
  virtual cdm::Status InitializeVideoDecoder(
      const cdm::VideoDecoderConfig& video_decoder_config) = 0;
  virtual void DeinitializeDecoder(cdm::StreamType decoder_type) = 0;
  virtual void ResetDecoder(cdm::StreamType decoder_type) = 0;
  virtual cdm::Status DecryptAndDecodeFrame(
      const cdm::InputBuffer& encrypted_buffer,
      cdm::VideoFrame* video_frame) = 0;
  virtual cdm::Status DecryptAndDecodeSamples(
      const cdm::InputBuffer& encrypted_buffer,
      cdm::AudioFrames* audio_frames) = 0;
  virtual void OnPlatformChallengeResponse(
      const cdm::PlatformChallengeResponse& response) = 0;
  virtual void OnQueryOutputProtectionStatus(
      uint32_t link_mask,
      uint32_t output_protection_mask) = 0;

  // ContentDecryptionModule_1 and ContentDecryptionModule_2 interface methods
  // AddKey() and CancelKeyRequest() (older versions of Update() and Close(),
  // respectively) pass in the session_id rather than the reference_id. As well,
  // Host_1 and Host_2 callbacks SendKeyMessage() and SendKeyError() include the
  // session ID, but the actual callbacks need the reference ID.
  //
  // The following functions maintain the reference_id <-> session_id mapping.
  // These can be removed once _1 and _2 interfaces are no longer supported.

  // Determine the corresponding reference_id for |session_id|.
  virtual uint32_t DetermineReferenceId(const std::string& session_id) = 0;

  // Determine the corresponding session_id for |reference_id|.
  virtual const std::string LookupSessionId(uint32_t reference_id) = 0;

 protected:
  typedef std::map<uint32_t, std::string> SessionMap;
  static const uint32_t kInvalidReferenceId = 0;

  CdmWrapper() : current_key_request_reference_id_(kInvalidReferenceId) {}

  // Map between session_id and reference_id.
  SessionMap session_map_;

  // As the response from GenerateKeyRequest() may be synchronous or
  // asynchronous, keep track of the current request during the call to handle
  // synchronous responses or errors. If no response received, add this request
  // to a queue and assume that the subsequent responses come back in the order
  // issued.
  // TODO(jrummell): Remove once all supported CDM host interfaces support
  // reference_id.
  uint32_t current_key_request_reference_id_;
  std::queue<uint32_t> pending_key_request_reference_ids_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmWrapper);
};

// Template class that does the CdmWrapper -> CdmInterface conversion. Default
// implementations are provided. Any methods that need special treatment should
// be specialized.
template <class CdmInterface>
class CdmWrapperImpl : public CdmWrapper {
 public:
  static CdmWrapper* Create(const char* key_system,
                            uint32_t key_system_size,
                            GetCdmHostFunc get_cdm_host_func,
                            void* user_data) {
    void* cdm_instance = ::CreateCdmInstance(
        CdmInterface::kVersion, key_system, key_system_size, get_cdm_host_func,
        user_data);
    if (!cdm_instance)
      return NULL;

    return new CdmWrapperImpl<CdmInterface>(
        static_cast<CdmInterface*>(cdm_instance));
  }

  virtual ~CdmWrapperImpl() {
    cdm_->Destroy();
  }

  // TODO(jrummell): In CDM_3 all key callbacks will use reference_id, so there
  // is no need to keep track of the current/pending request IDs. As well, the
  // definition for AddKey() and CancelKeyRequest() require the CDM to always
  // send a response (success or error), so the callbacks are not required.
  // Simplify the following 3 routines when CDM_3 is supported.

  virtual void GenerateKeyRequest(uint32_t reference_id,
                                  const char* type,
                                  uint32_t type_size,
                                  const uint8_t* init_data,
                                  uint32_t init_data_size) OVERRIDE {
    // As it is possible for CDMs to reply synchronously during the call to
    // GenerateKeyRequest(), keep track of |reference_id|.
    current_key_request_reference_id_ = reference_id;

    cdm::Status status =
        cdm_->GenerateKeyRequest(type, type_size, init_data, init_data_size);
    PP_DCHECK(status == cdm::kSuccess || status == cdm::kSessionError);
    if (status != cdm::kSuccess) {
      // If GenerateKeyRequest() failed, no subsequent asynchronous replies
      // will be sent. Verify that a response was sent synchronously.
      PP_DCHECK(current_key_request_reference_id_ == kInvalidReferenceId);
      current_key_request_reference_id_ = kInvalidReferenceId;
      return;
    }

    if (current_key_request_reference_id_) {
      // If this request is still pending (SendKeyMessage() or SendKeyError()
      // not called synchronously), add |reference_id| to the end of the queue.
      // Without CDM support, it is impossible to match SendKeyMessage()
      // (or SendKeyError()) responses to the |reference_id|. Doing the best
      // we can by keeping track of this in a queue, and assuming the responses
      // come back in order.
      pending_key_request_reference_ids_.push(reference_id);
      current_key_request_reference_id_ = kInvalidReferenceId;
    }
  }

  virtual Result AddKey(uint32_t reference_id,
                        const uint8_t* key,
                        uint32_t key_size,
                        const uint8_t* key_id,
                        uint32_t key_id_size) OVERRIDE {
    const std::string session_id = LookupSessionId(reference_id);
    if (session_id.empty()) {
      // Possible if AddKey() called before GenerateKeyRequest().
      return CALL_KEY_ERROR;
    }

    cdm::Status status = cdm_->AddKey(session_id.data(),
                                      session_id.size(),
                                      key,
                                      key_size,
                                      key_id,
                                      key_id_size);
    PP_DCHECK(status == cdm::kSuccess || status == cdm::kSessionError);
    if (status != cdm::kSuccess) {
      // http://crbug.com/310345: CDMs should send a KeyError message if they
      // return a failure, so no need to do it twice. Remove this once all CDMs
      // have been updated.
      return CALL_KEY_ERROR;
    }

    return CALL_KEY_ADDED;
  }

  virtual Result CancelKeyRequest(uint32_t reference_id) OVERRIDE {
    const std::string session_id = LookupSessionId(reference_id);
    if (session_id.empty()) {
      // Possible if CancelKeyRequest() called before GenerateKeyRequest().
      return CALL_KEY_ERROR;
    }

    session_map_.erase(reference_id);
    cdm::Status status =
        cdm_->CancelKeyRequest(session_id.data(), session_id.size());

    PP_DCHECK(status == cdm::kSuccess || status == cdm::kSessionError);
    if (status != cdm::kSuccess) {
      // http://crbug.com/310345: CDMs should send a KeyError message if they
      // return a failure, so no need to do it twice. Remove this once all CDMs
      // have been updated.
      return CALL_KEY_ERROR;
    }

    return NO_ACTION;
  }

  virtual void TimerExpired(void* context) OVERRIDE {
    cdm_->TimerExpired(context);
  }

  virtual cdm::Status Decrypt(const cdm::InputBuffer& encrypted_buffer,
                              cdm::DecryptedBlock* decrypted_buffer) OVERRIDE {
    return cdm_->Decrypt(encrypted_buffer, decrypted_buffer);
  }

  virtual cdm::Status InitializeAudioDecoder(
      const cdm::AudioDecoderConfig& audio_decoder_config) OVERRIDE {
    return cdm_->InitializeAudioDecoder(audio_decoder_config);
  }

  virtual cdm::Status InitializeVideoDecoder(
      const cdm::VideoDecoderConfig& video_decoder_config) OVERRIDE {
    return cdm_->InitializeVideoDecoder(video_decoder_config);
  }

  virtual void DeinitializeDecoder(cdm::StreamType decoder_type) OVERRIDE {
    cdm_->DeinitializeDecoder(decoder_type);
  }

  virtual void ResetDecoder(cdm::StreamType decoder_type) OVERRIDE {
    cdm_->ResetDecoder(decoder_type);
  }

  virtual cdm::Status DecryptAndDecodeFrame(
      const cdm::InputBuffer& encrypted_buffer,
      cdm::VideoFrame* video_frame) OVERRIDE {
    return cdm_->DecryptAndDecodeFrame(encrypted_buffer, video_frame);
  }

  virtual cdm::Status DecryptAndDecodeSamples(
      const cdm::InputBuffer& encrypted_buffer,
      cdm::AudioFrames* audio_frames) OVERRIDE {
    return cdm_->DecryptAndDecodeSamples(encrypted_buffer, audio_frames);
  }

  virtual void OnPlatformChallengeResponse(
      const cdm::PlatformChallengeResponse& response) OVERRIDE {
    cdm_->OnPlatformChallengeResponse(response);
  }

  virtual void OnQueryOutputProtectionStatus(
      uint32_t link_mask,
      uint32_t output_protection_mask) OVERRIDE {
    cdm_->OnQueryOutputProtectionStatus(link_mask, output_protection_mask);
  }

  uint32_t DetermineReferenceId(const std::string& session_id) {
    for (SessionMap::iterator it = session_map_.begin();
         it != session_map_.end();
         ++it) {
      if (it->second == session_id)
        return it->first;
    }

    // There is no entry in the map; assume it came from the current
    // GenerateKeyRequest() call (if possible). If no current request,
    // assume it came from the oldest GenerateKeyRequest() call.
    uint32_t reference_id = current_key_request_reference_id_;
    if (current_key_request_reference_id_) {
      // Only 1 response is allowed for the current GenerateKeyRequest().
      current_key_request_reference_id_ = kInvalidReferenceId;
    } else {
      PP_DCHECK(!pending_key_request_reference_ids_.empty());
      reference_id = pending_key_request_reference_ids_.front();
      pending_key_request_reference_ids_.pop();
    }

    // If this is a valid |session_id|, add it to the list. Otherwise, avoid
    // adding empty string as a mapping to prevent future calls with an empty
    // string from using the wrong reference_id.
    if (!session_id.empty()) {
      PP_DCHECK(session_map_.find(reference_id) == session_map_.end());
      PP_DCHECK(!session_id.empty());
      session_map_[reference_id] = session_id;
    }

    return reference_id;
  }

  const std::string LookupSessionId(uint32_t reference_id) {
    // Session may not exist if error happens during GenerateKeyRequest().
    SessionMap::iterator it = session_map_.find(reference_id);
    return (it != session_map_.end()) ? it->second : std::string();
  }

 private:
  CdmWrapperImpl(CdmInterface* cdm) : cdm_(cdm) {
    PP_DCHECK(cdm_);
  }

  CdmInterface* cdm_;

  DISALLOW_COPY_AND_ASSIGN(CdmWrapperImpl);
};

// Specializations for ContentDecryptionModule_1.

template <> void CdmWrapperImpl<cdm::ContentDecryptionModule_1>::
    OnPlatformChallengeResponse(
        const cdm::PlatformChallengeResponse& response) {
  PP_NOTREACHED();
}

template <> void CdmWrapperImpl<cdm::ContentDecryptionModule_1>::
    OnQueryOutputProtectionStatus(uint32_t link_mask,
                                  uint32_t output_protection_mask) {
  PP_NOTREACHED();
}

template <> cdm::Status CdmWrapperImpl<cdm::ContentDecryptionModule_1>::
    DecryptAndDecodeSamples(const cdm::InputBuffer& encrypted_buffer,
                            cdm::AudioFrames* audio_frames) {
  AudioFramesImpl audio_frames_1;
  cdm::Status status =
      cdm_->DecryptAndDecodeSamples(encrypted_buffer, &audio_frames_1);
  if (status != cdm::kSuccess)
    return status;

  audio_frames->SetFrameBuffer(audio_frames_1.PassFrameBuffer());
  audio_frames->SetFormat(cdm::kAudioFormatS16);
  return cdm::kSuccess;
}

CdmWrapper* CdmWrapper::Create(const char* key_system,
                               uint32_t key_system_size,
                               GetCdmHostFunc get_cdm_host_func,
                               void* user_data) {
  COMPILE_ASSERT(cdm::ContentDecryptionModule::kVersion ==
                 cdm::ContentDecryptionModule_2::kVersion,
                 update_code_below);

  // Ensure IsSupportedCdmInterfaceVersion matches this implementation.
  // Always update this DCHECK when updating this function.
  // If this check fails, update this function and DCHECK or update
  // IsSupportedCdmInterfaceVersion.
  PP_DCHECK(
      !IsSupportedCdmInterfaceVersion(
          cdm::ContentDecryptionModule::kVersion + 1) &&
      IsSupportedCdmInterfaceVersion(cdm::ContentDecryptionModule::kVersion) &&
      IsSupportedCdmInterfaceVersion(
          cdm::ContentDecryptionModule_1::kVersion) &&
      !IsSupportedCdmInterfaceVersion(
          cdm::ContentDecryptionModule_1::kVersion - 1));

  // Try to create the CDM using the latest CDM interface version.
  CdmWrapper* cdm_wrapper =
      CdmWrapperImpl<cdm::ContentDecryptionModule>::Create(
          key_system, key_system_size, get_cdm_host_func, user_data);
  if (cdm_wrapper)
    return cdm_wrapper;

  // Try to see if the CDM supports older version(s) of the CDM interface.
  cdm_wrapper = CdmWrapperImpl<cdm::ContentDecryptionModule_1>::Create(
      key_system, key_system_size, get_cdm_host_func, user_data);
  return cdm_wrapper;
}

// When updating the CdmAdapter, ensure you've updated the CdmWrapper to contain
// stub implementations for new or modified methods that the older CDM interface
// does not have.
// Also update supported_cdm_versions.h.
COMPILE_ASSERT(cdm::ContentDecryptionModule::kVersion ==
                   cdm::ContentDecryptionModule_2::kVersion,
               ensure_cdm_wrapper_templates_have_old_version_support);

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_CDM_WRAPPER_H_
