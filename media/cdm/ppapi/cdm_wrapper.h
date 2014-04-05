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
  static CdmWrapper* Create(const char* key_system,
                            uint32_t key_system_size,
                            GetCdmHostFunc get_cdm_host_func,
                            void* user_data);

  virtual ~CdmWrapper() {};

  virtual void CreateSession(uint32_t session_id,
                             const char* content_type,
                             uint32_t content_type_size,
                             const uint8_t* init_data,
                             uint32_t init_data_size) = 0;
  virtual void LoadSession(uint32_t session_id,
                           const char* web_session_id,
                           uint32_t web_session_id_size) = 0;
  virtual void UpdateSession(uint32_t session_id,
                             const uint8_t* response,
                             uint32_t response_size) = 0;
  virtual void ReleaseSession(uint32_t session_id) = 0;
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

 protected:
  CdmWrapper() {}

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

  virtual void CreateSession(uint32_t session_id,
                             const char* content_type,
                             uint32_t content_type_size,
                             const uint8_t* init_data,
                             uint32_t init_data_size) OVERRIDE {
    cdm_->CreateSession(
        session_id, content_type, content_type_size, init_data, init_data_size);
  }

  virtual void LoadSession(uint32_t session_id,
                           const char* web_session_id,
                           uint32_t web_session_id_size) OVERRIDE {
    cdm_->LoadSession(session_id, web_session_id, web_session_id_size);
  }

  virtual void UpdateSession(uint32_t session_id,
                             const uint8_t* response,
                             uint32_t response_size) OVERRIDE {
    cdm_->UpdateSession(session_id, response, response_size);
  }

  virtual void ReleaseSession(uint32_t session_id) OVERRIDE {
    cdm_->ReleaseSession(session_id);
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

 private:
  CdmWrapperImpl(CdmInterface* cdm) : cdm_(cdm) {
    PP_DCHECK(cdm_);
  }

  CdmInterface* cdm_;

  DISALLOW_COPY_AND_ASSIGN(CdmWrapperImpl);
};

CdmWrapper* CdmWrapper::Create(const char* key_system,
                               uint32_t key_system_size,
                               GetCdmHostFunc get_cdm_host_func,
                               void* user_data) {
  COMPILE_ASSERT(cdm::ContentDecryptionModule::kVersion ==
                 cdm::ContentDecryptionModule_4::kVersion,
                 update_code_below);

  // Ensure IsSupportedCdmInterfaceVersion() matches this implementation.
  // Always update this DCHECK when updating this function.
  // If this check fails, update this function and DCHECK or update
  // IsSupportedCdmInterfaceVersion().
  PP_DCHECK(
      !IsSupportedCdmInterfaceVersion(cdm::ContentDecryptionModule::kVersion +
                                      1) &&
      IsSupportedCdmInterfaceVersion(cdm::ContentDecryptionModule::kVersion) &&
      !IsSupportedCdmInterfaceVersion(cdm::ContentDecryptionModule::kVersion -
                                      1));

  // Try to create the CDM using the latest CDM interface version.
  CdmWrapper* cdm_wrapper =
      CdmWrapperImpl<cdm::ContentDecryptionModule>::Create(
          key_system, key_system_size, get_cdm_host_func, user_data);

  // If |cdm_wrapper| is NULL, try to create the CDM using older supported
  // versions of the CDM interface.
  // No older versions of CDM interface supported.

  return cdm_wrapper;
}

// When updating the CdmAdapter, ensure you've updated the CdmWrapper to contain
// stub implementations for new or modified methods that the older CDM interface
// does not have.
// Also update supported_cdm_versions.h.
COMPILE_ASSERT(cdm::ContentDecryptionModule::kVersion ==
                   cdm::ContentDecryptionModule_4::kVersion,
               ensure_cdm_wrapper_templates_have_old_version_support);

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_CDM_WRAPPER_H_
