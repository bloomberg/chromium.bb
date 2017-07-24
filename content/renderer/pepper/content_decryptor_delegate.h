// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_CONTENT_DECRYPTOR_DELEGATE_H_
#define CONTENT_RENDERER_PEPPER_CONTENT_DECRYPTOR_DELEGATE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/audio_buffer.h"
#include "media/base/cdm_promise.h"
#include "media/base/cdm_promise_adapter.h"
#include "media/base/cdm_session_tracker.h"
#include "media/base/channel_layout.h"
#include "media/base/content_decryption_module.h"
#include "media/base/decryptor.h"
#include "media/base/sample_format.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/private/pp_content_decryptor.h"
#include "ppapi/c/private/ppp_content_decryptor_private.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class AudioDecoderConfig;
class DecoderBuffer;
class VideoDecoderConfig;
}

namespace content {

class PPB_Buffer_Impl;

class ContentDecryptorDelegate {
 public:
  // ContentDecryptorDelegate does not take ownership of
  // |plugin_decryption_interface|. Therefore |plugin_decryption_interface|
  // must outlive this object.
  ContentDecryptorDelegate(
      PP_Instance pp_instance,
      const PPP_ContentDecryptor_Private* plugin_decryption_interface);
  ~ContentDecryptorDelegate();

  // This object should not be accessed after |fatal_plugin_error_cb| is called.
  void Initialize(
      const std::string& key_system,
      bool allow_distinctive_identifier,
      bool allow_persistent_state,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb,
      const base::Closure& fatal_plugin_error_cb,
      std::unique_ptr<media::SimpleCdmPromise> promise);

  void InstanceCrashed();

  // Provides access to PPP_ContentDecryptor_Private.
  void SetServerCertificate(const std::vector<uint8_t>& certificate,
                            std::unique_ptr<media::SimpleCdmPromise> promise);
  void GetStatusForPolicy(media::HdcpVersion min_hdcp_version,
                          std::unique_ptr<media::KeyStatusCdmPromise> promise);
  void CreateSessionAndGenerateRequest(
      media::CdmSessionType session_type,
      media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<media::NewSessionCdmPromise> promise);
  void LoadSession(media::CdmSessionType session_type,
                   const std::string& session_id,
                   std::unique_ptr<media::NewSessionCdmPromise> promise);
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     std::unique_ptr<media::SimpleCdmPromise> promise);
  void CloseSession(const std::string& session_id,
                    std::unique_ptr<media::SimpleCdmPromise> promise);
  void RemoveSession(const std::string& session_id,
                     std::unique_ptr<media::SimpleCdmPromise> promise);
  bool Decrypt(media::Decryptor::StreamType stream_type,
               const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
               const media::Decryptor::DecryptCB& decrypt_cb);
  bool CancelDecrypt(media::Decryptor::StreamType stream_type);
  bool InitializeAudioDecoder(
      const media::AudioDecoderConfig& decoder_config,
      const media::Decryptor::DecoderInitCB& decoder_init_cb);
  bool InitializeVideoDecoder(
      const media::VideoDecoderConfig& decoder_config,
      const media::Decryptor::DecoderInitCB& decoder_init_cb);
  // TODO(tomfinegan): Add callback args for DeinitializeDecoder() and
  // ResetDecoder()
  bool DeinitializeDecoder(media::Decryptor::StreamType stream_type);
  bool ResetDecoder(media::Decryptor::StreamType stream_type);
  // Note: These methods can be used with unencrypted data.
  bool DecryptAndDecodeAudio(
      const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
      const media::Decryptor::AudioDecodeCB& audio_decode_cb);
  bool DecryptAndDecodeVideo(
      const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
      const media::Decryptor::VideoDecodeCB& video_decode_cb);

  // PPB_ContentDecryptor_Private dispatching methods.
  void OnPromiseResolved(uint32_t promise_id);
  void OnPromiseResolvedWithKeyStatus(uint32_t promise_id,
                                      PP_CdmKeyStatus key_status);
  void OnPromiseResolvedWithSession(uint32_t promise_id, PP_Var session_id);
  void OnPromiseRejected(uint32_t promise_id,
                         PP_CdmExceptionCode exception_code,
                         uint32_t system_code,
                         PP_Var error_description);
  void OnSessionMessage(PP_Var session_id,
                        PP_CdmMessageType message_type,
                        PP_Var message);
  void OnSessionKeysChange(PP_Var session_id,
                           PP_Bool has_additional_usable_key,
                           uint32_t key_count,
                           const struct PP_KeyInformation key_information[]);
  void OnSessionExpirationChange(PP_Var session_id, PP_Time new_expiry_time);
  void OnSessionClosed(PP_Var session_id);
  void DeliverBlock(PP_Resource decrypted_block,
                    const PP_DecryptedBlockInfo* block_info);
  void DecoderInitializeDone(PP_DecryptorStreamType decoder_type,
                             uint32_t request_id,
                             PP_Bool success);
  void DecoderDeinitializeDone(PP_DecryptorStreamType decoder_type,
                               uint32_t request_id);
  void DecoderResetDone(PP_DecryptorStreamType decoder_type,
                        uint32_t request_id);
  void DeliverFrame(PP_Resource decrypted_frame,
                    const PP_DecryptedFrameInfo* frame_info);
  void DeliverSamples(PP_Resource audio_frames,
                      const PP_DecryptedSampleInfo* sample_info);

 private:
  template <typename Callback>
  class TrackableCallback {
   public:
    TrackableCallback() : id_(0u) {}
    ~TrackableCallback() {
      // Callbacks must be satisfied.
      DCHECK_EQ(id_, 0u);
      DCHECK(is_null());
    };

    bool Matches(uint32_t id) const { return id == id_; }

    bool is_null() const { return cb_.is_null(); }

    void Set(uint32_t id, const Callback& cb) {
      DCHECK_EQ(id_, 0u);
      DCHECK(cb_.is_null());
      id_ = id;
      cb_ = cb;
    }

    Callback ResetAndReturn() {
      id_ = 0;
      return base::ResetAndReturn(&cb_);
    }

   private:
    uint32_t id_;
    Callback cb_;
  };

  // Cancels the pending decrypt-and-decode callback for |stream_type|.
  void CancelDecode(media::Decryptor::StreamType stream_type);

  // Fills |resource| with a PPB_Buffer_Impl and copies the data from
  // |encrypted_buffer| into the buffer resource. This method reuses
  // |audio_input_resource_| and |video_input_resource_| to reduce the latency
  // in requesting new PPB_Buffer_Impl resources. The caller must make sure that
  // |audio_input_resource_| or |video_input_resource_| is available before
  // calling this method.
  //
  // An end of stream |encrypted_buffer| is represented as a null |resource|.
  //
  // Returns true upon success and false if any error happened.
  bool MakeMediaBufferResource(
      media::Decryptor::StreamType stream_type,
      const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
      scoped_refptr<PPB_Buffer_Impl>* resource);

  void FreeBuffer(uint32_t buffer_id);

  void SetBufferToFreeInTrackingInfo(PP_DecryptTrackingInfo* tracking_info);

  // Deserializes audio data stored in |audio_frames| into individual audio
  // buffers in |frames|. Returns true upon success.
  bool DeserializeAudioFrames(PP_Resource audio_frames,
                              size_t data_size,
                              media::SampleFormat sample_format,
                              media::Decryptor::AudioFrames* frames);

  void SatisfyAllPendingCallbacksOnError();

  const PP_Instance pp_instance_;
  const PPP_ContentDecryptor_Private* const plugin_decryption_interface_;

  // TODO(ddorwin): Remove after updating the Pepper API to not use key system.
  std::string key_system_;

  // Callbacks for firing session events.
  media::SessionMessageCB session_message_cb_;
  media::SessionClosedCB session_closed_cb_;
  media::SessionKeysChangeCB session_keys_change_cb_;
  media::SessionExpirationUpdateCB session_expiration_update_cb_;

  // Callback to notify that unexpected error happened and |this| should not
  // be used anymore.
  base::Closure fatal_plugin_error_cb_;

  gfx::Size natural_size_;

  // Request ID for tracking pending content decryption callbacks.
  // Note that zero indicates an invalid request ID.
  // TODO(xhwang): Add completion callbacks for Reset/Stop and remove the use
  // of request IDs.
  uint32_t next_decryption_request_id_;

  TrackableCallback<media::Decryptor::DecryptCB> audio_decrypt_cb_;
  TrackableCallback<media::Decryptor::DecryptCB> video_decrypt_cb_;
  TrackableCallback<media::Decryptor::DecoderInitCB> audio_decoder_init_cb_;
  TrackableCallback<media::Decryptor::DecoderInitCB> video_decoder_init_cb_;
  TrackableCallback<media::Decryptor::AudioDecodeCB> audio_decode_cb_;
  TrackableCallback<media::Decryptor::VideoDecodeCB> video_decode_cb_;

  // Cached audio and video input buffers. See MakeMediaBufferResource.
  scoped_refptr<PPB_Buffer_Impl> audio_input_resource_;
  scoped_refptr<PPB_Buffer_Impl> video_input_resource_;

  std::queue<uint32_t> free_buffers_;

  // Keep track of audio parameters.
  int audio_samples_per_second_;
  int audio_channel_count_;
  media::ChannelLayout audio_channel_layout_;

  media::CdmPromiseAdapter cdm_promise_adapter_;

  media::CdmSessionTracker cdm_session_tracker_;

  scoped_refptr<media::AudioBufferMemoryPool> pool_;

  base::WeakPtr<ContentDecryptorDelegate> weak_this_;
  base::WeakPtrFactory<ContentDecryptorDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentDecryptorDelegate);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_CONTENT_DECRYPTOR_DELEGATE_H_
