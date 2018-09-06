// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_D3D11_VIDEO_DECODER_H_
#define MEDIA_GPU_D3D11_VIDEO_DECODER_H_

#include <string>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/callback_registry.h"
#include "media/base/video_decoder.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_create_device_cb.h"
#include "media/gpu/windows/d3d11_h264_accelerator.h"

namespace gpu {
class CommandBufferStub;
}  // namespace gpu

namespace media {

class D3D11PictureBuffer;
class D3D11VideoDecoderImpl;
class D3D11VideoDecoderTest;
class MediaLog;

// Video decoder that uses D3D11 directly.  It is intended that this class will
// run the decoder on whatever thread it lives on.  However, at the moment, it
// only works if it's on the gpu main thread.
class MEDIA_GPU_EXPORT D3D11VideoDecoder : public VideoDecoder,
                                           public D3D11VideoDecoderClient {
 public:
  // |get_stub_cb| must be called from |gpu_task_runner|.
  static std::unique_ptr<VideoDecoder> Create(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      std::unique_ptr<MediaLog> media_log,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb);

  // VideoDecoder implementation:
  std::string GetDisplayName() const override;
  void Initialize(
      const VideoDecoderConfig& config,
      bool low_delay,
      CdmContext* cdm_context,
      const InitCB& init_cb,
      const OutputCB& output_cb,
      const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::RepeatingClosure& closure) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;

  // D3D11VideoDecoderClient implementation.
  D3D11PictureBuffer* GetPicture() override;
  void OutputResult(D3D11PictureBuffer* buffer,
                    const VideoColorSpace& buffer_colorspace) override;

  // Return false |config| definitely isn't going to work, so that we can fail
  // init without bothering with a thread hop.
  bool IsPotentiallySupported(const VideoDecoderConfig& config);

  // Override how we create D3D11 devices, to inject mocks.
  void SetCreateDeviceCallbackForTesting(D3D11CreateDeviceCB callback);

 protected:
  // Owners should call Destroy(). This is automatic via
  // std::default_delete<media::VideoDecoder> when held by a
  // std::unique_ptr<media::VideoDecoder>.
  ~D3D11VideoDecoder() override;

 private:
  friend class D3D11VideoDecoderTest;

  D3D11VideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      std::unique_ptr<MediaLog> media_log,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      std::unique_ptr<D3D11VideoDecoderImpl> impl,
      base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb);

  // Receive |buffer|, that is now unused by the client.
  void ReceivePictureBufferFromClient(scoped_refptr<D3D11PictureBuffer> buffer);

  // Called when the gpu side of initialization is complete.
  void OnGpuInitComplete(bool success);

  // Run the decoder loop.
  void DoDecode();

  // Create new PictureBuffers.  Currently, this completes synchronously, but
  // really should have an async interface since it must do some work on the gpu
  // main thread.
  void CreatePictureBuffers();

  enum class D3D11VideoNotSupportedReason {
    kVideoIsSupported = 0,

    // D3D11 version 11.1 required.
    kInsufficientD3D11FeatureLevel = 1,

    // The video profile is not supported .
    kProfileNotSupported = 2,

    // GPU options: require zero copy.
    kZeroCopyNv12Required = 3,

    // GPU options: require zero copy.
    kZeroCopyVideoRequired = 4,

    // The video codec must be H264.
    kCodecNotSupported = 5,

    // For UMA. Must be the last entry. It should be initialized to the
    // numerically largest value above; if you add more entries, then please
    // update this to the last one.
    kMaxValue = kCodecNotSupported
  };

  std::unique_ptr<MediaLog> media_log_;

  enum class State {
    // Initializing resources required to create a codec.
    kInitializing,
    // Initialization has completed and we're running. This is the only state
    // in which |codec_| might be non-null. If |codec_| is null, a codec
    // creation is pending.
    kRunning,
    // The decoder cannot make progress because it doesn't have the key to
    // decrypt the buffer. Waiting for a new key to be available.
    // This should only be transitioned from kRunning, and should only
    // transition to kRunning.
    kWaitingForNewKey,
    // A fatal error occurred. A terminal state.
    kError,
  };

  // Record a UMA about why IsPotentiallySupported returned false, or that it
  // returned true.  Also will add a MediaLog entry, etc.
  void SetWasSupportedReason(D3D11VideoNotSupportedReason enum_value);

  // Callback to notify that new usable key is available.
  void NotifyNewKey();

  // Enter the kError state.  This will fail any pending |init_cb_| and / or
  // pending decode as well.
  void NotifyError(const char* reason);

  // The implementation, which we trampoline to the impl thread.
  // This must be freed on the impl thread.
  std::unique_ptr<D3D11VideoDecoderImpl> impl_;

  // Weak ptr to |impl_|, which we use for callbacks.
  base::WeakPtr<D3D11VideoDecoderImpl> impl_weak_;

  // Task runner for |impl_|.  This must be the GPU main thread.
  scoped_refptr<base::SequencedTaskRunner> impl_task_runner_;

  gpu::GpuPreferences gpu_preferences_;
  gpu::GpuDriverBugWorkarounds gpu_workarounds_;

  // During init, these will be set.
  InitCB init_cb_;
  OutputCB output_cb_;
  bool is_encrypted_ = false;

  D3D11CreateDeviceCB create_device_func_;

  Microsoft::WRL::ComPtr<ID3D11Device> device_;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context_;
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext1> video_context_;

  std::unique_ptr<AcceleratedVideoDecoder> accelerated_video_decoder_;

  GUID decoder_guid_;

  std::list<std::pair<scoped_refptr<DecoderBuffer>, DecodeCB>>
      input_buffer_queue_;
  scoped_refptr<DecoderBuffer> current_buffer_;
  DecodeCB current_decode_cb_;
  base::TimeDelta current_timestamp_;

  // Callback registration to keep the new key callback registered.
  std::unique_ptr<CallbackRegistration> new_key_callback_registration_;

  // Must be called on the gpu main thread.  So, don't call it from here, since
  // we don't know what thread we're on.
  base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb_;

  // It would be nice to unique_ptr these, but we give a ref to the VideoFrame
  // so that the texture is retained until the mailbox is opened.
  std::vector<scoped_refptr<D3D11PictureBuffer>> picture_buffers_;

  State state_ = State::kInitializing;

  // Entire class should be single-sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<D3D11VideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(D3D11VideoDecoder);
};

}  // namespace media

#endif  // MEDIA_GPU_D3D11_VIDEO_DECODER_H_
