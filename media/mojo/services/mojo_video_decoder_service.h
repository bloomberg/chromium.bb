// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_VIDEO_DECODER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_VIDEO_DECODER_SERVICE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "media/base/decode_status.h"
#include "media/base/overlay_info.h"
#include "media/base/video_decoder.h"
#include "media/mojo/interfaces/video_decoder.mojom.h"
#include "media/mojo/services/mojo_media_client.h"

namespace gpu {
struct SyncToken;
}

namespace media {

class ContentDecryptionModule;
class DecoderBuffer;
class MojoCdmServiceContext;
class MojoDecoderBufferReader;
class MojoMediaClient;
class MojoMediaLog;
class VideoFrame;

// Implementation of a mojom::VideoDecoder which runs in the GPU process,
// and wraps a media::VideoDecoder.
class MojoVideoDecoderService : public mojom::VideoDecoder {
 public:
  explicit MojoVideoDecoderService(
      MojoMediaClient* mojo_media_client,
      base::WeakPtr<MojoCdmServiceContext> mojo_cdm_service_context);
  ~MojoVideoDecoderService() final;

  // mojom::VideoDecoder implementation
  void Construct(mojom::VideoDecoderClientAssociatedPtrInfo client,
                 mojom::MediaLogAssociatedPtrInfo media_log,
                 mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
                 mojom::CommandBufferIdPtr command_buffer_id) final;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  int32_t cdm_id,
                  InitializeCallback callback) final;
  void Decode(mojom::DecoderBufferPtr buffer, DecodeCallback callback) final;
  void Reset(ResetCallback callback) final;
  void OnReleaseMailbox(const base::UnguessableToken& release_token,
                        const gpu::SyncToken& release_sync_token) final;
  void OnOverlayInfoChanged(const OverlayInfo& overlay_info) final;

 private:
  // Helper methods so that we can bind them with a weak pointer to avoid
  // running mojom::VideoDecoder callbacks after connection error happens and
  // |this| is deleted. It's not safe to run the callbacks after a connection
  // error.
  void OnDecoderInitialized(InitializeCallback callback,
                            scoped_refptr<ContentDecryptionModule> cdm,
                            bool success);
  void OnDecoderRead(DecodeCallback callback,
                     scoped_refptr<DecoderBuffer> buffer);
  void OnDecoderDecoded(DecodeCallback callback, DecodeStatus status);
  void OnDecoderReset(ResetCallback callback);
  void OnDecoderOutputWithReleaseCB(
      MojoMediaClient::ReleaseMailboxCB release_cb,
      const scoped_refptr<VideoFrame>& frame);
  void OnDecoderOutput(base::Optional<base::UnguessableToken> release_token,
                       const scoped_refptr<VideoFrame>& frame);

  void OnDecoderRequestedOverlayInfo(
      bool restart_for_transitions,
      const ProvideOverlayInfoCB& provide_overlay_info_cb);

  mojom::VideoDecoderClientAssociatedPtr client_;
  std::unique_ptr<MojoMediaLog> media_log_;
  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader_;

  MojoMediaClient* mojo_media_client_;
  std::unique_ptr<media::VideoDecoder> decoder_;
  std::map<base::UnguessableToken, MojoMediaClient::ReleaseMailboxCB>
      release_mailbox_cbs_;
  ProvideOverlayInfoCB provide_overlay_info_cb_;

  // A helper object required to get CDM from CDM id.
  base::WeakPtr<MojoCdmServiceContext> mojo_cdm_service_context_;

  // Hold a reference to the CDM to keep it alive for the lifetime of the
  // |decoder_|. The |cdm_| owns the CdmContext which is passed to |decoder_|.
  scoped_refptr<ContentDecryptionModule> cdm_;

  base::WeakPtr<MojoVideoDecoderService> weak_this_;
  base::WeakPtrFactory<MojoVideoDecoderService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoVideoDecoderService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_VIDEO_DECODER_SERVICE_H_
