// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_SERVICE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/audio_decoder.h"
#include "media/mojo/interfaces/audio_decoder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"

namespace media {

class ContentDecryptionModule;
class MojoCdmServiceContext;
class MojoDecoderBufferReader;

class MEDIA_MOJO_EXPORT MojoAudioDecoderService : public mojom::AudioDecoder {
 public:
  MojoAudioDecoderService(MojoCdmServiceContext* mojo_cdm_service_context,
                          std::unique_ptr<media::AudioDecoder> decoder);

  ~MojoAudioDecoderService() final;

  // mojom::AudioDecoder implementation
  void Construct(mojom::AudioDecoderClientAssociatedPtrInfo client) final;
  void Initialize(const AudioDecoderConfig& config,
                  int32_t cdm_id,
                  InitializeCallback callback) final;

  void SetDataSource(mojo::ScopedDataPipeConsumerHandle receive_pipe) final;

  void Decode(mojom::DecoderBufferPtr buffer, DecodeCallback callback) final;

  void Reset(ResetCallback callback) final;

 private:
  // Called by |decoder_| upon finishing initialization.
  void OnInitialized(InitializeCallback callback,
                     scoped_refptr<ContentDecryptionModule> cdm,
                     bool success);

  // Called by |mojo_decoder_buffer_reader_| when read is finished.
  void OnReadDone(DecodeCallback callback, scoped_refptr<DecoderBuffer> buffer);

  // Called by |mojo_decoder_buffer_reader_| when reset is finished.
  void OnReaderFlushDone(ResetCallback callback);

  // Called by |decoder_| when DecoderBuffer is accepted or rejected.
  void OnDecodeStatus(DecodeCallback callback, media::DecodeStatus status);

  // Called by |decoder_| when reset sequence is finished.
  void OnResetDone(ResetCallback callback);

  // Called by |decoder_| for each decoded buffer.
  void OnAudioBufferReady(const scoped_refptr<AudioBuffer>& audio_buffer);

  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader_;

  // A helper object required to get CDM from CDM id.
  MojoCdmServiceContext* const mojo_cdm_service_context_ = nullptr;

  // The destination for the decoded buffers.
  mojom::AudioDecoderClientAssociatedPtr client_;

  // Hold a reference to the CDM to keep it alive for the lifetime of the
  // |decoder_|. The |cdm_| owns the CdmContext which is passed to |decoder_|.
  scoped_refptr<ContentDecryptionModule> cdm_;

  // The AudioDecoder that does actual decoding work.
  // This MUST be declared after |cdm_| to maintain correct destruction order.
  // The |decoder_| may need to access the CDM to do some clean up work in its
  // own destructor.
  std::unique_ptr<media::AudioDecoder> decoder_;

  base::WeakPtr<MojoAudioDecoderService> weak_this_;
  base::WeakPtrFactory<MojoAudioDecoderService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoAudioDecoderService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_SERVICE_H_
