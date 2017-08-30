// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CDM_PPAPI_DECRYPTOR_H_
#define CONTENT_RENDERER_MEDIA_CDM_PPAPI_DECRYPTOR_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "content/renderer/media/cdm/pepper_cdm_wrapper.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_factory.h"
#include "media/base/content_decryption_module.h"
#include "media/base/decryptor.h"
#include "media/base/video_decoder_config.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace url {
class Origin;
}

namespace content {
class ContentDecryptorDelegate;

// PpapiDecryptor implements media::ContentDecryptionModule and media::Decryptor
// and forwards all calls to the PluginInstance.
// This class should always be created & destroyed on the main renderer thread.
class PpapiDecryptor : public media::ContentDecryptionModule,
                       public media::CdmContext,
                       public media::Decryptor {
 public:
  static void Create(
      const std::string& key_system,
      const url::Origin& security_origin,
      bool allow_distinctive_identifier,
      bool allow_persistent_state,
      const CreatePepperCdmCB& create_pepper_cdm_cb,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb,
      const media::CdmCreatedCB& cdm_created_cb);

  // media::ContentDecryptionModule implementation.
  void SetServerCertificate(
      const std::vector<uint8_t>& certificate,
      std::unique_ptr<media::SimpleCdmPromise> promise) override;
  void GetStatusForPolicy(
      media::HdcpVersion min_hdcp_version,
      std::unique_ptr<media::KeyStatusCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      media::CdmSessionType session_type,
      media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<media::NewSessionCdmPromise> promise) override;
  void LoadSession(
      media::CdmSessionType session_type,
      const std::string& session_id,
      std::unique_ptr<media::NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     std::unique_ptr<media::SimpleCdmPromise> promise) override;
  void CloseSession(const std::string& session_id,
                    std::unique_ptr<media::SimpleCdmPromise> promise) override;
  void RemoveSession(const std::string& session_id,
                     std::unique_ptr<media::SimpleCdmPromise> promise) override;
  CdmContext* GetCdmContext() override;

  // media::CdmContext implementation.
  Decryptor* GetDecryptor() override;
  int GetCdmId() const override;

  // media::Decryptor implementation.
  void RegisterNewKeyCB(StreamType stream_type,
                        const NewKeyCB& key_added_cb) override;
  void Decrypt(StreamType stream_type,
               const scoped_refptr<media::DecoderBuffer>& encrypted,
               const DecryptCB& decrypt_cb) override;
  void CancelDecrypt(StreamType stream_type) override;
  void InitializeAudioDecoder(const media::AudioDecoderConfig& config,
                              const DecoderInitCB& init_cb) override;
  void InitializeVideoDecoder(const media::VideoDecoderConfig& config,
                              const DecoderInitCB& init_cb) override;
  void DecryptAndDecodeAudio(
      const scoped_refptr<media::DecoderBuffer>& encrypted,
      const AudioDecodeCB& audio_decode_cb) override;
  void DecryptAndDecodeVideo(
      const scoped_refptr<media::DecoderBuffer>& encrypted,
      const VideoDecodeCB& video_decode_cb) override;
  void ResetDecoder(StreamType stream_type) override;
  void DeinitializeDecoder(StreamType stream_type) override;

 private:
  PpapiDecryptor(
      std::unique_ptr<PepperCdmWrapper> pepper_cdm_wrapper,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb);

  ~PpapiDecryptor() override;

  void InitializeCdm(const std::string& key_system,
                     bool allow_distinctive_identifier,
                     bool allow_persistent_state,
                     std::unique_ptr<media::SimpleCdmPromise> promise);

  void OnDecoderInitialized(StreamType stream_type, bool success);

  // Callbacks for |plugin_cdm_delegate_| to fire session events.
  void OnSessionMessage(const std::string& session_id,
                        media::CdmMessageType message_type,
                        const std::vector<uint8_t>& message);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           media::CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(const std::string& session_id,
                                 base::Time new_expiry_time);
  void OnSessionClosed(const std::string& session_id);

  void AttemptToResumePlayback();

  // Callback to notify that a fatal error happened in |plugin_cdm_delegate_|.
  // The error is terminal and |plugin_cdm_delegate_| should not be used after
  // this call.
  void OnFatalPluginError();

  ContentDecryptorDelegate* CdmDelegate();

  // Hold a reference of the Pepper CDM wrapper to make sure the plugin lives
  // as long as needed.
  std::unique_ptr<PepperCdmWrapper> pepper_cdm_wrapper_;

  // Callbacks for firing session events.
  media::SessionMessageCB session_message_cb_;
  media::SessionClosedCB session_closed_cb_;
  media::SessionKeysChangeCB session_keys_change_cb_;
  media::SessionExpirationUpdateCB session_expiration_update_cb_;

  scoped_refptr<base::SingleThreadTaskRunner> render_task_runner_;

  DecoderInitCB audio_decoder_init_cb_;
  DecoderInitCB video_decoder_init_cb_;
  NewKeyCB new_audio_key_cb_;
  NewKeyCB new_video_key_cb_;

  base::Lock lock_;  // Protects |had_fatal_plugin_error_|.
  bool had_fatal_plugin_error_ = false;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PpapiDecryptor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PpapiDecryptor);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CDM_PPAPI_DECRYPTOR_H_
