// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MEDIA_CODEC_VIDEO_DECODER_H_
#define MEDIA_GPU_ANDROID_MEDIA_CODEC_VIDEO_DECODER_H_

#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/timer/elapsed_timer.h"
#include "gpu/ipc/service/gpu_command_buffer_stub.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/base/overlay_info.h"
#include "media/base/video_decoder.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/device_info.h"
#include "media/gpu/android/video_frame_factory.h"
#include "media/gpu/android_video_surface_chooser.h"
#include "media/gpu/avda_codec_allocator.h"
#include "media/gpu/media_gpu_export.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace media {

struct PendingDecode {
  static PendingDecode CreateEos();
  PendingDecode(scoped_refptr<DecoderBuffer> buffer,
                VideoDecoder::DecodeCB decode_cb);
  PendingDecode(PendingDecode&& other);
  ~PendingDecode();

  scoped_refptr<DecoderBuffer> buffer;
  VideoDecoder::DecodeCB decode_cb;

 private:
  DISALLOW_COPY_AND_ASSIGN(PendingDecode);
};

// TODO(watk): Simplify the interface to AVDACodecAllocator.
struct CodecAllocatorAdapter
    : public AVDACodecAllocatorClient,
      public base::SupportsWeakPtr<CodecAllocatorAdapter> {
  using CodecCreatedCb =
      base::Callback<void(std::unique_ptr<MediaCodecBridge>)>;

  CodecAllocatorAdapter();
  ~CodecAllocatorAdapter();
  void OnCodecConfigured(
      std::unique_ptr<MediaCodecBridge> media_codec) override;

  CodecCreatedCb codec_created_cb;
};

// An Android VideoDecoder that delegates to MediaCodec.
class MEDIA_GPU_EXPORT MediaCodecVideoDecoder : public VideoDecoder {
 public:
  MediaCodecVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      base::Callback<gpu::GpuCommandBufferStub*()> get_stub_cb,
      VideoFrameFactory::OutputWithReleaseMailboxCB output_cb,
      DeviceInfo* device_info,
      AVDACodecAllocator* codec_allocator,
      std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
      std::unique_ptr<VideoFrameFactory> video_frame_factory,
      std::unique_ptr<service_manager::ServiceContextRef> connection_ref);

  // VideoDecoder implementation:
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) override;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;

  // Sets the overlay info to use. This can be called before Initialize() to
  // set the first overlay.
  void SetOverlayInfo(const OverlayInfo& overlay_info);

 protected:
  // Protected for testing.
  ~MediaCodecVideoDecoder() override;

 private:
  // The test has access for PumpCodec().
  friend class MediaCodecVideoDecoderTest;
  friend class base::DeleteHelper<MediaCodecVideoDecoder>;

  enum class State {
    kOk,
    kError,
    // We haven't initialized |surface_chooser_| yet, so we don't have a surface
    // or a codec.
    kBeforeSurfaceInit,
    // Set when we are waiting for a codec to be created.
    kWaitingForCodec,
    // Set when we have a codec, but it doesn't yet have a key.
    kWaitingForKey,
    // The output surface was destroyed. This is a terminal state like kError,
    // but it isn't reported as a decode error.
    kSurfaceDestroyed,
  };

  enum class DrainType {
    kForReset,
    kForDestroy,
  };

  // Starts teardown.
  void Destroy() override;

  // Finishes initialization.
  void StartLazyInit();
  void OnVideoFrameFactoryInitialized(
      scoped_refptr<SurfaceTextureGLOwner> surface_texture);

  // Initializes |surface_chooser_|.
  void InitializeSurfaceChooser();
  void OnSurfaceChosen(std::unique_ptr<AndroidOverlay> overlay);
  void OnSurfaceDestroyed(AndroidOverlay* overlay);

  // Sets |codecs_|'s output surface to |incoming_surface_|. Releases the codec
  // and both the current and incoming bundles on failure.
  void TransitionToIncomingSurface();
  void CreateCodec();
  void OnCodecCreated(std::unique_ptr<MediaCodecBridge> codec);

  // Flushes the codec, or if flush() is not supported, releases it and creates
  // a new one.
  void FlushCodec();

  // Attempts to queue input and dequeue output from the codec. If
  // |force_start_timer| is true the timer idle timeout is reset.
  void PumpCodec(bool force_start_timer);
  void ManageTimer(bool start_timer);
  bool QueueInput();
  bool DequeueOutput();

  // Forwards |frame| via |output_cb_| if there hasn't been a Reset() since the
  // frame was created (i.e., |reset_generation| matches |reset_generation_|).
  void ForwardVideoFrame(int reset_generation,
                         VideoFrameFactory::ReleaseMailboxCB release_cb,
                         const scoped_refptr<VideoFrame>& frame);

  // Starts draining the codec by queuing an EOS if required. It skips the drain
  // if possible.
  void StartDrainingCodec(DrainType drain_type);
  void OnCodecDrained();

  void ClearPendingDecodes(DecodeStatus status);

  // Sets |state_| and runs pending callbacks.
  void HandleError();

  // Releases |codec_| if it's not null.
  void ReleaseCodec();

  // Calls ReleaseCodec() and drops the ref to its surface bundle.
  void ReleaseCodecAndBundle();

  // Creates an overlay factory cb based on the value of overlay_info_.
  AndroidOverlayFactoryCB CreateOverlayFactoryCb();

  State state_;

  // Whether initialization still needs to be done on the first decode call.
  bool lazy_init_pending_;
  std::deque<PendingDecode> pending_decodes_;

  // The reason for the current drain operation if any.
  base::Optional<DrainType> drain_type_;

  // The current reset cb if a Reset() is in progress.
  base::Closure reset_cb_;

  // A generation counter that's incremented every time Reset() is called.
  int reset_generation_;

  // The EOS decode cb for an EOS currently being processed by the codec. Called
  // when the EOS is output.
  VideoDecoder::DecodeCB eos_decode_cb_;

  VideoFrameFactory::OutputWithReleaseMailboxCB output_cb_;
  VideoDecoderConfig decoder_config_;

  // The surface bundle that we're transitioning to, if any.
  base::Optional<scoped_refptr<AVDASurfaceBundle>> incoming_surface_;

  // |codec_config_| must not be modified while |state_| is kWaitingForCodec.
  scoped_refptr<CodecConfig> codec_config_;
  std::unique_ptr<CodecWrapper> codec_;
  base::Optional<base::ElapsedTimer> idle_timer_;
  base::RepeatingTimer pump_codec_timer_;

  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  base::Callback<gpu::GpuCommandBufferStub*()> get_stub_cb_;

  // An adapter to let us use AVDACodecAllocator.
  CodecAllocatorAdapter codec_allocator_adapter_;
  AVDACodecAllocator* codec_allocator_;

  // A SurfaceTexture that is kept for the lifetime of MCVD so that if we have
  // to synchronously switch surfaces we always have one available.
  // TODO: Remove this once onSurfaceDestroyed() callbacks are not delivered
  // via the gpu thread. We can't post a task to the gpu thread to
  // create a SurfaceTexture inside the onSurfaceDestroyed() handler without
  // deadlocking currently, because the gpu thread might be blocked waiting
  // for the SurfaceDestroyed to be handled.
  scoped_refptr<SurfaceTextureGLOwner> surface_texture_;

  // The current overlay info, which possibly specifies an overlay to render to.
  OverlayInfo overlay_info_;

  // The surface chooser we use to decide which kind of surface to configure the
  // codec with.
  std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser_;

  // Current state for the chooser.
  AndroidVideoSurfaceChooser::State chooser_state_;

  // The factory for creating VideoFrames from CodecOutputBuffers.
  std::unique_ptr<VideoFrameFactory> video_frame_factory_;

  // An optional factory callback for creating mojo AndroidOverlays.
  AndroidOverlayMojoFactoryCB overlay_factory_cb_;

  DeviceInfo* device_info_;

  // If we're running in a service context this ref lets us keep the service
  // thread alive until destruction.
  std::unique_ptr<service_manager::ServiceContextRef> context_ref_;
  base::WeakPtrFactory<MediaCodecVideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecVideoDecoder);
};

}  // namespace media

namespace std {

// Specialize std::default_delete to call Destroy().
template <>
struct MEDIA_GPU_EXPORT default_delete<media::MediaCodecVideoDecoder>
    : public default_delete<media::VideoDecoder> {};

}  // namespace std

#endif  // MEDIA_GPU_ANDROID_MEDIA_CODEC_VIDEO_DECODER_H_
