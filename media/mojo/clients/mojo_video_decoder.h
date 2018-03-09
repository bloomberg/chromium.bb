// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_VIDEO_DECODER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_VIDEO_DECODER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/mojo/clients/mojo_media_log_service.h"
#include "media/mojo/interfaces/video_decoder.mojom.h"
#include "media/video/video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class GpuVideoAcceleratorFactories;
class MediaLog;
class MojoDecoderBufferWriter;
class MojoVideoFrameHandleReleaser;

// A VideoDecoder, for use in the renderer process, that proxies to a
// mojom::VideoDecoder. It is assumed that the other side will be implemented by
// MojoVideoDecoderService, running in the GPU process, and that the remote
// decoder will be hardware accelerated.
class MojoVideoDecoder final : public VideoDecoder,
                               public mojom::VideoDecoderClient {
 public:
  MojoVideoDecoder(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   GpuVideoAcceleratorFactories* gpu_factories,
                   MediaLog* media_log,
                   mojom::VideoDecoderPtr remote_decoder,
                   const RequestOverlayInfoCB& request_overlay_info_cb);
  ~MojoVideoDecoder() final;

  // VideoDecoder implementation.
  std::string GetDisplayName() const final;
  void Initialize(
      const VideoDecoderConfig& config,
      bool low_delay,
      CdmContext* cdm_context,
      const InitCB& init_cb,
      const OutputCB& output_cb,
      const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) final;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) final;
  void Reset(const base::Closure& closure) final;
  bool NeedsBitstreamConversion() const final;
  bool CanReadWithoutStalling() const final;
  int GetMaxDecodeRequests() const final;

  // mojom::VideoDecoderClient implementation.
  void OnVideoFrameDecoded(
      const scoped_refptr<VideoFrame>& frame,
      bool can_read_without_stalling,
      const base::Optional<base::UnguessableToken>& release_token) final;
  void RequestOverlayInfo(bool restart_for_transitions) final;

  void set_writer_capacity_for_testing(uint32_t capacity) {
    writer_capacity_ = capacity;
  }

 private:
  void OnInitializeDone(bool status,
                        bool needs_bitstream_conversion,
                        int32_t max_decode_requests);
  void OnDecodeDone(uint64_t decode_id, DecodeStatus status);
  void OnResetDone();

  void BindRemoteDecoder();

  // Forwards |overlay_info| to the remote decoder.
  void OnOverlayInfoChanged(const OverlayInfo& overlay_info);

  // Cleans up callbacks and blocks future calls.
  void Stop();

  // Task runner that the decoder runs on (media thread).
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Used to pass the remote decoder from the constructor (on the main thread)
  // to Initialize() (on the media thread).
  mojom::VideoDecoderPtrInfo remote_decoder_info_;

  // Manages VideoFrame destruction callbacks.
  scoped_refptr<MojoVideoFrameHandleReleaser> mojo_video_frame_handle_releaser_;

  GpuVideoAcceleratorFactories* gpu_factories_ = nullptr;

  InitCB init_cb_;
  OutputCB output_cb_;
  uint64_t decode_counter_ = 0;
  std::map<uint64_t, DecodeCB> pending_decodes_;
  base::Closure reset_cb_;

  mojom::VideoDecoderPtr remote_decoder_;
  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer_;

  uint32_t writer_capacity_ = 0;

  bool remote_decoder_bound_ = false;
  bool has_connection_error_ = false;
  mojo::AssociatedBinding<mojom::VideoDecoderClient> client_binding_;
  MojoMediaLogService media_log_service_;
  mojo::AssociatedBinding<mojom::MediaLog> media_log_binding_;
  RequestOverlayInfoCB request_overlay_info_cb_;
  bool overlay_info_requested_ = false;

  bool initialized_ = false;
  bool needs_bitstream_conversion_ = false;
  bool can_read_without_stalling_ = true;
  int32_t max_decode_requests_ = 1;

  base::WeakPtr<MojoVideoDecoder> weak_this_;
  base::WeakPtrFactory<MojoVideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_VIDEO_DECODER_H_
