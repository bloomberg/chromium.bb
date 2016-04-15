// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_H_
#define MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder.h"
#include "media/mojo/interfaces/audio_decoder.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// An AudioDecoder that proxies to an interfaces::AudioDecoder.
class MojoAudioDecoder : public AudioDecoder,
                         public interfaces::AudioDecoderClient {
 public:
  MojoAudioDecoder(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   interfaces::AudioDecoderPtr remote_decoder);
  ~MojoAudioDecoder() final;

  // AudioDecoder implementation.
  std::string GetDisplayName() const final;
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) final;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) final;
  void Reset(const base::Closure& closure) final;
  bool NeedsBitstreamConversion() const final;

  // AudioDecoderClient implementation.
  void OnBufferDecoded(interfaces::AudioBufferPtr buffer) final;

 private:
  // Callback for connection error on |remote_decoder_|.
  void OnConnectionError();

  // Called when |remote_decoder_| finished initialization.
  void OnInitialized(bool success, bool needs_bitstream_conversion);

  // Called when |remote_decoder_| accepted or rejected DecoderBuffer.
  void OnDecodeStatus(interfaces::AudioDecoder::DecodeStatus decode_status);

  // called when |remote_decoder_| finished Reset() sequence.
  void OnResetDone();

  // A helper method that creates data pipe and sets the data connection to
  // the service.
  void CreateDataPipe();

  // A helper method to serialize the data section of DecoderBuffer into pipe.
  interfaces::DecoderBufferPtr TransferDecoderBuffer(
      const scoped_refptr<DecoderBuffer>& media_buffer);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // This class is constructed on one thread and used exclusively on another
  // thread. This member is used to safely pass the AudioDecoderPtr from one
  // thread to another. It is set in the constructor and is consumed in
  // Initialize().
  interfaces::AudioDecoderPtrInfo remote_decoder_info_;

  interfaces::AudioDecoderPtr remote_decoder_;

  // DataPipe for serializing the data section of DecoderBuffer.
  mojo::ScopedDataPipeProducerHandle producer_handle_;

  // Binding for AudioDecoderClient, bound to the |task_runner_|.
  mojo::Binding<AudioDecoderClient> binding_;

  // We call the following callbacks to pass the information to the pipeline.
  // |output_cb_| is permanent while other three are called only once,
  // |decode_cb_| and |reset_cb_| are replaced by every by Decode() and Reset().
  InitCB init_cb_;
  OutputCB output_cb_;
  DecodeCB decode_cb_;
  base::Closure reset_cb_;

  // Flag that is set if we got connection error. Never cleared.
  bool has_connection_error_;

  // Flag telling whether this decoder requires bitstream conversion.
  // Passed from |remote_decoder_| as a result of its initialization.
  bool needs_bitstream_conversion_;

  DISALLOW_COPY_AND_ASSIGN(MojoAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_H_
