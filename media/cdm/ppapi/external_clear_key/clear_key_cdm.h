// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_CLEAR_KEY_CDM_H_
#define MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_CLEAR_KEY_CDM_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/cdm/ppapi/external_clear_key/cdm_host_proxy.h"
#include "media/cdm/ppapi/external_clear_key/clear_key_persistent_session_cdm.h"

namespace media {

class CdmVideoDecoder;
class DecoderBuffer;
class FFmpegCdmAudioDecoder;
class FileIOTestRunner;

// Clear key implementation of the cdm::ContentDecryptionModule interface.
class ClearKeyCdm : public cdm::ContentDecryptionModule, public CdmHostProxy {
 public:
  using Host = cdm::ContentDecryptionModule::Host;

  ClearKeyCdm(Host* host, const std::string& key_system);
  ~ClearKeyCdm() override;

  // cdm::ContentDecryptionModule implementation.
  void Initialize(bool allow_distinctive_identifier,
                  bool allow_persistent_state) override;
  void GetStatusForPolicy(uint32_t promise_id,
                          const cdm::Policy& policy) override;
  void CreateSessionAndGenerateRequest(uint32_t promise_id,
                                       cdm::SessionType session_type,
                                       cdm::InitDataType init_data_type,
                                       const uint8_t* init_data,
                                       uint32_t init_data_size) override;
  void LoadSession(uint32_t promise_id,
                   cdm::SessionType session_type,
                   const char* session_id,
                   uint32_t session_id_length) override;
  void UpdateSession(uint32_t promise_id,
                     const char* session_id,
                     uint32_t session_id_length,
                     const uint8_t* response,
                     uint32_t response_size) override;
  void CloseSession(uint32_t promise_id,
                    const char* session_id,
                    uint32_t session_id_length) override;
  void RemoveSession(uint32_t promise_id,
                     const char* session_id,
                     uint32_t session_id_length) override;
  void SetServerCertificate(uint32_t promise_id,
                            const uint8_t* server_certificate_data,
                            uint32_t server_certificate_data_size) override;
  void TimerExpired(void* context) override;
  cdm::Status Decrypt(const cdm::InputBuffer& encrypted_buffer,
                      cdm::DecryptedBlock* decrypted_block) override;
  cdm::Status InitializeAudioDecoder(
      const cdm::AudioDecoderConfig& audio_decoder_config) override;
  cdm::Status InitializeVideoDecoder(
      const cdm::VideoDecoderConfig& video_decoder_config) override;
  void DeinitializeDecoder(cdm::StreamType decoder_type) override;
  void ResetDecoder(cdm::StreamType decoder_type) override;
  cdm::Status DecryptAndDecodeFrame(const cdm::InputBuffer& encrypted_buffer,
                                    cdm::VideoFrame* video_frame) override;
  cdm::Status DecryptAndDecodeSamples(const cdm::InputBuffer& encrypted_buffer,
                                      cdm::AudioFrames* audio_frames) override;
  void Destroy() override;
  void OnPlatformChallengeResponse(
      const cdm::PlatformChallengeResponse& response) override;
  void OnQueryOutputProtectionStatus(cdm::QueryResult result,
                                     uint32_t link_mask,
                                     uint32_t output_protection_mask) override;
  void OnStorageId(uint32_t version,
                   const uint8_t* storage_id,
                   uint32_t storage_id_size) override;

  // CdmHostProxy implementation.
  cdm::Buffer* Allocate(uint32_t capacity) override;
  cdm::FileIO* CreateFileIO(cdm::FileIOClient* client) override;

 private:
  // ContentDecryptionModule callbacks.
  void OnSessionMessage(const std::string& session_id,
                        CdmMessageType message_type,
                        const std::vector<uint8_t>& message);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info);
  void OnSessionClosed(const std::string& session_id);
  void OnSessionExpirationUpdate(const std::string& session_id,
                                 base::Time new_expiry_time);

  // Handle the success/failure of a promise. These methods are responsible for
  // calling |host_| to resolve or reject the promise.
  void OnSessionCreated(uint32_t promise_id, const std::string& session_id);
  void OnPromiseResolved(uint32_t promise_id);
  void OnPromiseFailed(uint32_t promise_id,
                       CdmPromise::Exception exception_code,
                       uint32_t system_code,
                       const std::string& error_message);

  // After updating a session send a 'expirationChange' event.
  void OnUpdateSuccess(uint32_t promise_id, const std::string& session_id);

  // Prepares next renewal message and sets a timer for it.
  void ScheduleNextRenewal();

  // Decrypts the |encrypted_buffer| and puts the result in |decrypted_buffer|.
  // Returns cdm::kSuccess if decryption succeeded. The decrypted result is
  // put in |decrypted_buffer|. If |encrypted_buffer| is empty, the
  // |decrypted_buffer| is set to an empty (EOS) buffer.
  // Returns cdm::kNoKey if no decryption key was available. In this case
  // |decrypted_buffer| should be ignored by the caller.
  // Returns cdm::kDecryptError if any decryption error occurred. In this case
  // |decrypted_buffer| should be ignored by the caller.
  cdm::Status DecryptToMediaDecoderBuffer(
      const cdm::InputBuffer& encrypted_buffer,
      scoped_refptr<DecoderBuffer>* decrypted_buffer);

  void OnUnitTestComplete(bool success);

  void StartFileIOTest();

  // Callback for CDM File IO test.
  void OnFileIOTestComplete(bool success);

  void StartOutputProtectionTest();
  void StartPlatformVerificationTest();
  void VerifyCdmHostTest();
  void StartStorageIdTest();

  Host* const host_;
  scoped_refptr<ContentDecryptionModule> cdm_;

  const std::string key_system_;
  bool allow_persistent_state_;

  std::string last_session_id_;
  std::string next_renewal_message_;

  // Timer delay in milliseconds for the next host_->SetTimer() call.
  int64_t timer_delay_ms_;

  // Indicates whether a renewal timer has been set to prevent multiple timers
  // from running.
  bool renewal_timer_set_;

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  std::unique_ptr<FFmpegCdmAudioDecoder> audio_decoder_;
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER

  std::unique_ptr<CdmVideoDecoder> video_decoder_;

  std::unique_ptr<FileIOTestRunner> file_io_test_runner_;

  bool is_running_output_protection_test_;
  bool is_running_platform_verification_test_;
  bool is_running_storage_id_test_;

  DISALLOW_COPY_AND_ASSIGN(ClearKeyCdm);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_CLEAR_KEY_CDM_H_
