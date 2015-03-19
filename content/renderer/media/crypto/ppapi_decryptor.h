// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/renderer/media/crypto/pepper_cdm_wrapper.h"
#include "media/base/cdm_context.h"
#include "media/base/decryptor.h"
#include "media/base/media_keys.h"
#include "media/base/video_decoder_config.h"

class GURL;

namespace base {
class MessageLoopProxy;
}

namespace content {
class ContentDecryptorDelegate;
class PepperPluginInstanceImpl;

// PpapiDecryptor implements media::MediaKeys and media::Decryptor and forwards
// all calls to the PluginInstance.
// This class should always be created & destroyed on the main renderer thread.
class PpapiDecryptor : public media::MediaKeys,
                       public media::CdmContext,
                       public media::Decryptor {
 public:
  static scoped_ptr<PpapiDecryptor> Create(
      const std::string& key_system,
      bool allow_distinctive_identifier,
      bool allow_persistent_state,
      const GURL& security_origin,
      const CreatePepperCdmCB& create_pepper_cdm_cb,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionErrorCB& session_error_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb);

  ~PpapiDecryptor() override;

  // media::MediaKeys implementation.
  void SetServerCertificate(
      const uint8* certificate_data,
      int certificate_data_length,
      scoped_ptr<media::SimpleCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      SessionType session_type,
      const std::string& init_data_type,
      const uint8* init_data,
      int init_data_length,
      scoped_ptr<media::NewSessionCdmPromise> promise) override;
  void LoadSession(SessionType session_type,
                   const std::string& session_id,
                   scoped_ptr<media::NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const uint8* response,
                     int response_length,
                     scoped_ptr<media::SimpleCdmPromise> promise) override;
  void CloseSession(const std::string& session_id,
                    scoped_ptr<media::SimpleCdmPromise> promise) override;
  void RemoveSession(const std::string& session_id,
                     scoped_ptr<media::SimpleCdmPromise> promise) override;
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
      const std::string& key_system,
      bool allow_distinctive_identifier,
      bool allow_persistent_state,
      scoped_ptr<PepperCdmWrapper> pepper_cdm_wrapper,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionErrorCB& session_error_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb);

  void OnDecoderInitialized(StreamType stream_type, bool success);

  // Callbacks for |plugin_cdm_delegate_| to fire session events.
  void OnSessionMessage(const std::string& session_id,
                        MediaKeys::MessageType message_type,
                        const std::vector<uint8>& message,
                        const GURL& legacy_destination_url);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           media::CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(const std::string& session_id,
                                 const base::Time& new_expiry_time);
  void OnSessionClosed(const std::string& session_id);
  void OnSessionError(const std::string& session_id,
                      MediaKeys::Exception exception_code,
                      uint32 system_code,
                      const std::string& error_description);

  void AttemptToResumePlayback();

  // Callback to notify that a fatal error happened in |plugin_cdm_delegate_|.
  // The error is terminal and |plugin_cdm_delegate_| should not be used after
  // this call.
  void OnFatalPluginError();

  ContentDecryptorDelegate* CdmDelegate();

  // Hold a reference of the Pepper CDM wrapper to make sure the plugin lives
  // as long as needed.
  scoped_ptr<PepperCdmWrapper> pepper_cdm_wrapper_;

  // Callbacks for firing session events.
  media::SessionMessageCB session_message_cb_;
  media::SessionClosedCB session_closed_cb_;
  media::SessionErrorCB session_error_cb_;
  media::SessionKeysChangeCB session_keys_change_cb_;
  media::SessionExpirationUpdateCB session_expiration_update_cb_;

  scoped_refptr<base::MessageLoopProxy> render_loop_proxy_;

  DecoderInitCB audio_decoder_init_cb_;
  DecoderInitCB video_decoder_init_cb_;
  NewKeyCB new_audio_key_cb_;
  NewKeyCB new_video_key_cb_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PpapiDecryptor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PpapiDecryptor);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_
