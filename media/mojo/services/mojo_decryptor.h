// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_H_
#define MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/decryptor.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace media {

// A Decryptor implementation based on interfaces::DecryptorPtr.
// This class is single threaded. The |remote_decryptor| is connected before
// being passed to MojoDecryptor, but it is bound to the thread MojoDecryptor
// lives on the first time it is used in this class.
class MojoDecryptor : public Decryptor {
 public:
  explicit MojoDecryptor(interfaces::DecryptorPtr remote_decryptor);
  ~MojoDecryptor() final;

  // Decryptor implementation.
  void RegisterNewKeyCB(StreamType stream_type,
                        const NewKeyCB& key_added_cb) final;
  void Decrypt(StreamType stream_type,
               const scoped_refptr<DecoderBuffer>& encrypted,
               const DecryptCB& decrypt_cb) final;
  void CancelDecrypt(StreamType stream_type) final;
  void InitializeAudioDecoder(const AudioDecoderConfig& config,
                              const DecoderInitCB& init_cb) final;
  void InitializeVideoDecoder(const VideoDecoderConfig& config,
                              const DecoderInitCB& init_cb) final;
  void DecryptAndDecodeAudio(const scoped_refptr<DecoderBuffer>& encrypted,
                             const AudioDecodeCB& audio_decode_cb) final;
  void DecryptAndDecodeVideo(const scoped_refptr<DecoderBuffer>& encrypted,
                             const VideoDecodeCB& video_decode_cb) final;
  void ResetDecoder(StreamType stream_type) final;
  void DeinitializeDecoder(StreamType stream_type) final;

  // Called when keys have changed and an additional key is available.
  void OnKeyAdded();

 private:
  // Called when a buffer is decrypted.
  void OnBufferDecrypted(const DecryptCB& decrypt_cb,
                         interfaces::Decryptor::Status status,
                         interfaces::DecoderBufferPtr buffer);
  void OnAudioDecoded(const AudioDecodeCB& audio_decode_cb,
                      interfaces::Decryptor::Status status,
                      mojo::Array<interfaces::AudioBufferPtr> audio_buffers);
  void OnVideoDecoded(const VideoDecodeCB& video_decode_cb,
                      interfaces::Decryptor::Status status,
                      interfaces::VideoFramePtr video_frame);

  // To pass DecoderBuffers to and from the MojoDecryptorService, 2 data pipes
  // are required (one each way). At initialization both pipes are created,
  // and then the handles are passed to the MojoDecryptorService.
  void CreateDataPipes();

  // Helper functions to write and read a DecoderBuffer.
  interfaces::DecoderBufferPtr TransferDecoderBuffer(
      const scoped_refptr<DecoderBuffer>& buffer);
  scoped_refptr<DecoderBuffer> ReadDecoderBuffer(
      interfaces::DecoderBufferPtr buffer);

  base::ThreadChecker thread_checker_;

  interfaces::DecryptorPtr remote_decryptor_;

  // DataPipes for serializing the data section of DecoderBuffer into/from.
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  NewKeyCB new_audio_key_cb_;
  NewKeyCB new_video_key_cb_;

  base::WeakPtrFactory<MojoDecryptor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoDecryptor);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_H_
