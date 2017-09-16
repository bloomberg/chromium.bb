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
#include "media/gpu/android/android_video_surface_chooser.h"
#include "media/gpu/android/avda_codec_allocator.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/device_info.h"
#include "media/gpu/android/video_frame_factory.h"
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

// An Android VideoDecoder that delegates to MediaCodec.
class MEDIA_GPU_EXPORT MediaCodecVideoDecoder
    : public VideoDecoder,
      public AVDACodecAllocatorClient {
 public:
  MediaCodecVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      base::Callback<gpu::GpuCommandBufferStub*()> get_stub_cb,
      VideoFrameFactory::OutputWithReleaseMailboxCB output_cb,
      DeviceInfo* device_info,
      AVDACodecAllocator* codec_allocator,
      std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
      AndroidOverlayMojoFactoryCB overlay_factory_cb,
      RequestOverlayInfoCB request_overlay_info_cb,
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

  // Updates the current overlay info.
  void OnOverlayInfoChanged(const OverlayInfo& overlay_info);

 protected:
  // Protected for testing.
  ~MediaCodecVideoDecoder() override;

 private:
  // The test has access for PumpCodec().
  friend class MediaCodecVideoDecoderTest;
  friend class base::DeleteHelper<MediaCodecVideoDecoder>;

  enum class State {
    // We're initializing.
    kInitializing,
    // Initialization has completed and we're running. This is the only state
    // in which |codec_| might be non-null. If |codec_| is null, a codec
    // creation is pending.
    kRunning,
    // A fatal error occurred. A terminal state.
    kError,
    // The output surface was destroyed, but SetOutputSurface() is not supported
    // by the device. In this case the consumer is responsible for destroying us
    // soon, so this is terminal state but not a decode error.
    kSurfaceDestroyed
  };

  enum class DrainType { kForReset, kForDestroy };

  // Starts teardown.
  void Destroy() override;

  // Finishes initialization.
  void StartLazyInit();
  void OnVideoFrameFactoryInitialized(
      scoped_refptr<SurfaceTextureGLOwner> surface_texture);

  // Resets |waiting_for_key_| to false, indicating that MediaCodec might now
  // accept buffers.
  void OnKeyAdded();

  // Initializes |surface_chooser_|.
  void InitializeSurfaceChooser();
  void OnSurfaceChosen(std::unique_ptr<AndroidOverlay> overlay);
  void OnSurfaceDestroyed(AndroidOverlay* overlay);

  // Whether we have a codec and its surface is not equal to
  // |target_surface_bundle_|.
  bool SurfaceTransitionPending();

  // Sets |codecs_|'s output surface to |target_surface_bundle_|.
  void TransitionToTargetSurface();

  // Creates a codec asynchronously.
  void CreateCodec();

  // AVDACodecAllocatorClient implementation.
  void OnCodecConfigured(
      std::unique_ptr<MediaCodecBridge> media_codec,
      scoped_refptr<AVDASurfaceBundle> surface_bundle) override;

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

  // Sets |state_| and does common teardown for the terminal states. |state_|
  // must be either kSurfaceDestroyed or kError.
  void EnterTerminalState(State state);

  // Releases |codec_| if it's not null.
  void ReleaseCodec();

  // Creates an overlay factory cb based on the value of overlay_info_.
  AndroidOverlayFactoryCB CreateOverlayFactoryCb();

  State state_;

  // Whether initialization still needs to be done on the first decode call.
  bool lazy_init_pending_;
  std::deque<PendingDecode> pending_decodes_;

  // Whether we've seen MediaCodec return MEDIA_CODEC_NO_KEY indicating that
  // the corresponding key was not set yet, and MediaCodec will not accept
  // buffers until OnKeyAdded() is called.
  bool waiting_for_key_;

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

  // Codec specific data (SPS and PPS for H264). Some MediaCodecs initialize
  // more reliably if we explicitly pass these (http://crbug.com/649185).
  std::vector<uint8_t> csd0_;
  std::vector<uint8_t> csd1_;

  // The surface bundle that we should transition to if a transition is pending.
  scoped_refptr<AVDASurfaceBundle> incoming_surface_;

  std::unique_ptr<CodecWrapper> codec_;
  base::Optional<base::ElapsedTimer> idle_timer_;
  base::RepeatingTimer pump_codec_timer_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  base::Callback<gpu::GpuCommandBufferStub*()> get_stub_cb_;
  AVDACodecAllocator* codec_allocator_;

  // The current target surface that |codec_| should be rendering to. It
  // reflects the latest surface choice by |surface_chooser_|. If the codec is
  // configured with some other surface, then a transition is pending. It's
  // non-null from the first surface choice.
  scoped_refptr<AVDASurfaceBundle> target_surface_bundle_;

  // A SurfaceTexture bundle that is kept for the lifetime of MCVD so that if we
  // have to synchronously switch surfaces we always have one available.
  scoped_refptr<AVDASurfaceBundle> surface_texture_bundle_;

  // A callback for requesting overlay info updates.
  RequestOverlayInfoCB request_overlay_info_cb_;

  // The current overlay info, which possibly specifies an overlay to render to.
  OverlayInfo overlay_info_;

  // The surface chooser we use to decide which kind of surface to configure the
  // codec with.
  std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser_;

  // Current state for the chooser.
  AndroidVideoSurfaceChooser::State chooser_state_;

  // The factory for creating VideoFrames from CodecOutputBuffers.
  std::unique_ptr<VideoFrameFactory> video_frame_factory_;

  // An optional factory callback for creating mojo AndroidOverlays. This must
  // only be called on the GPU thread.
  AndroidOverlayMojoFactoryCB overlay_factory_cb_;

  DeviceInfo* device_info_;

  // If we're running in a service context this ref lets us keep the service
  // thread alive until destruction.
  std::unique_ptr<service_manager::ServiceContextRef> context_ref_;
  base::WeakPtrFactory<MediaCodecVideoDecoder> weak_factory_;
  base::WeakPtrFactory<MediaCodecVideoDecoder> codec_allocator_weak_factory_;

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
