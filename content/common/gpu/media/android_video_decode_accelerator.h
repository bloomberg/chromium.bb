// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_ANDROID_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_ANDROID_VIDEO_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <list>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/common/gpu/media/avda_state_provider.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "media/base/media_keys.h"
#include "media/video/video_decode_accelerator.h"

namespace gfx {
class SurfaceTexture;
}

namespace content {

// A VideoDecodeAccelerator implementation for Android.
// This class decodes the input encoded stream by using Android's MediaCodec
// class. http://developer.android.com/reference/android/media/MediaCodec.html
// It delegates attaching pictures to PictureBuffers to a BackingStrategy, but
// otherwise handles the work of transferring data to / from MediaCodec.
class CONTENT_EXPORT AndroidVideoDecodeAccelerator
    : public media::VideoDecodeAccelerator,
      public AVDAStateProvider {
 public:
  typedef std::map<int32_t, media::PictureBuffer> OutputBufferMap;

  // A BackingStrategy is responsible for making a PictureBuffer's texture
  // contain the image that a MediaCodec decoder buffer tells it to.
  class BackingStrategy {
   public:
    virtual ~BackingStrategy() {}

    // Called after the state provider is given, but before any other
    // calls to the BackingStrategy.
    virtual void Initialize(AVDAStateProvider* provider) = 0;

    // Called before the AVDA does any Destroy() work.  This will be
    // the last call that the BackingStrategy receives.
    virtual void Cleanup(bool have_context,
                         const OutputBufferMap& buffer_map) = 0;

    // Return the GL texture target that the PictureBuffer textures use.
    virtual uint32_t GetTextureTarget() const = 0;

    // Create and return a surface texture for the MediaCodec to use.
    virtual scoped_refptr<gfx::SurfaceTexture> CreateSurfaceTexture() = 0;

    // Make the provided PictureBuffer draw the image that is represented by
    // the decoded output buffer at codec_buffer_index.
    virtual void UseCodecBufferForPictureBuffer(
        int32_t codec_buffer_index,
        const media::PictureBuffer& picture_buffer) = 0;

    // Notify strategy that a picture buffer has been assigned.
    virtual void AssignOnePictureBuffer(
        const media::PictureBuffer& picture_buffer) {}

    // Notify strategy that a picture buffer has been reused.
    virtual void ReuseOnePictureBuffer(
        const media::PictureBuffer& picture_buffer) {}

    // Notify strategy that we are about to dismiss a picture buffer.
    virtual void DismissOnePictureBuffer(
        const media::PictureBuffer& picture_buffer) {}

    // Notify strategy that we have a new android MediaCodec instance.  This
    // happens when we're starting up or re-configuring mid-stream.  Any
    // previously provided codec should no longer be referenced.
    // For convenience, a container of PictureBuffers is provided in case
    // per-image cleanup is needed.
    virtual void CodecChanged(media::VideoCodecBridge* codec,
                              const OutputBufferMap& buffer_map) = 0;

    // Notify the strategy that a frame is available.  This callback can happen
    // on any thread at any time.
    virtual void OnFrameAvailable() = 0;
  };

  AndroidVideoDecodeAccelerator(
      const base::WeakPtr<gpu::gles2::GLES2Decoder> decoder,
      const base::Callback<bool(void)>& make_context_current);

  ~AndroidVideoDecodeAccelerator() override;

  // media::VideoDecodeAccelerator implementation:
  bool Initialize(const Config& config, Client* client) override;
  void SetCdm(int cdm_id) override;
  void Decode(const media::BitstreamBuffer& bitstream_buffer) override;
  void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;
  bool CanDecodeOnIOThread() override;

  // AVDAStateProvider implementation:
  const gfx::Size& GetSize() const override;
  const base::ThreadChecker& ThreadChecker() const override;
  base::WeakPtr<gpu::gles2::GLES2Decoder> GetGlDecoder() const override;
  void PostError(const ::tracked_objects::Location& from_here,
                 media::VideoDecodeAccelerator::Error error) override;

  static media::VideoDecodeAccelerator::Capabilities GetCapabilities();

  // Notifies about SurfaceTexture::OnFrameAvailable.  This can happen on any
  // thread at any time!
  void OnFrameAvailable();

 private:
  // TODO(timav): evaluate the need for more states in the AVDA state machine.
  enum State {
    NO_ERROR,
    ERROR,
    WAITING_FOR_KEY,
  };

  static const base::TimeDelta kDecodePollDelay;

  // Configures |media_codec_| with the given codec parameters from the client.
  bool ConfigureMediaCodec();

  // Sends the decoded frame specified by |codec_buffer_index| to the client.
  void SendDecodedFrameToClient(int32_t codec_buffer_index,
                                int32_t bitstream_id);

  // Does pending IO tasks if any. Once this is called, it polls |media_codec_|
  // until it finishes pending tasks. For the polling, |kDecodePollDelay| is
  // used.
  void DoIOTask();

  // Feeds input data to |media_codec_|. This checks
  // |pending_bitstream_buffers_| and queues a buffer to |media_codec_|.
  // Returns true if any input was processed.
  bool QueueInput();

  // Dequeues output from |media_codec_| and feeds the decoded frame to the
  // client.  Returns a hint about whether calling again might produce
  // more output.
  bool DequeueOutput();

  // Requests picture buffers from the client.
  void RequestPictureBuffers();

  // This callback is called after CDM obtained a MediaCrypto object.
  void OnMediaCryptoReady(media::MediaDrmBridge::JavaObjectPtr media_crypto,
                          bool needs_protected_surface);

  // This callback is called when a new key is added to CDM.
  void OnKeyAdded();

  // Notifies the client of the CDM setting result.
  void NotifyCdmAttached(bool success);

  // Notifies the client about the availability of a picture.
  void NotifyPictureReady(const media::Picture& picture);

  // Notifies the client that the input buffer identifed by input_buffer_id has
  // been processed.
  void NotifyEndOfBitstreamBuffer(int input_buffer_id);

  // Notifies the client that the decoder was flushed.
  void NotifyFlushDone();

  // Notifies the client that the decoder was reset.
  void NotifyResetDone();

  // Notifies about decoding errors.
  // Note: you probably don't want to call this directly.  Use PostError or
  // RETURN_ON_FAILURE, since we can defer error reporting to keep the pipeline
  // from breaking.  NotifyError will do so immediately, PostError may wait.
  // |token| has to match |error_sequence_token_|, or else it's assumed to be
  // from a post that's prior to a previous reset, and ignored.
  void NotifyError(media::VideoDecodeAccelerator::Error error, int token);

  // Start or stop our work-polling timer based on whether we did any work, and
  // how long it has been since we've done work.  Calling this with true will
  // start the timer.  Calling it with false may stop the timer.
  void ManageTimer(bool did_work);

  // Resets MediaCodec and buffers/containers used for storing output. These
  // components need to be reset upon EOS to decode a later stream. Input state
  // (e.g. queued BitstreamBuffers) is not reset, as input following an EOS
  // is still valid and should be processed.
  void ResetCodecState();

  // Dismiss all |output_picture_buffers_| in preparation for requesting new
  // ones.
  void DismissPictureBuffers();

  // Return true if and only if we should use deferred rendering.
  static bool UseDeferredRenderingStrategy();

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // To expose client callbacks from VideoDecodeAccelerator.
  Client* client_;

  // Callback to set the correct gl context.
  base::Callback<bool(void)> make_context_current_;

  // Codec type. Used when we configure media codec.
  media::VideoCodec codec_;

  // Whether the stream is encrypted.
  bool is_encrypted_;

  // Whether encryption scheme requires to use protected surface.
  bool needs_protected_surface_;

  // The current state of this class. For now, this is used only for setting
  // error state.
  State state_;

  // This map maintains the picture buffers passed to the client for decoding.
  // The key is the picture buffer id.
  OutputBufferMap output_picture_buffers_;

  // This keeps the free picture buffer ids which can be used for sending
  // decoded frames to the client.
  std::queue<int32_t> free_picture_ids_;

  // Picture buffer ids which have been dismissed and not yet re-assigned.  Used
  // to ignore ReusePictureBuffer calls that were in flight when the
  // DismissPictureBuffer call was made.
  std::set<int32_t> dismissed_picture_ids_;

  // The low-level decoder which Android SDK provides.
  scoped_ptr<media::VideoCodecBridge> media_codec_;

  // A container of texture. Used to set a texture to |media_codec_|.
  scoped_refptr<gfx::SurfaceTexture> surface_texture_;

  // Set to true after requesting picture buffers to the client.
  bool picturebuffers_requested_;

  // The resolution of the stream.
  gfx::Size size_;

  // Encoded bitstream buffers to be passed to media codec, queued until an
  // input buffer is available, along with the time when they were first
  // enqueued.
  typedef std::queue<std::pair<media::BitstreamBuffer, base::Time> >
      PendingBitstreamBuffers;
  PendingBitstreamBuffers pending_bitstream_buffers_;

  // A map of presentation timestamp to bitstream buffer id for the bitstream
  // buffers that have been submitted to the decoder but haven't yet produced an
  // output frame with the same timestamp. Note: there will only be one entry
  // for multiple bitstream buffers that have the same presentation timestamp.
  std::map<base::TimeDelta, int32_t> bitstream_buffers_in_decoder_;

  // Keeps track of bitstream ids notified to the client with
  // NotifyEndOfBitstreamBuffer() before getting output from the bitstream.
  std::list<int32_t> bitstreams_notified_in_advance_;

  // Owner of the GL context. Used to restore the context state.
  base::WeakPtr<gpu::gles2::GLES2Decoder> gl_decoder_;

  // Repeating timer responsible for draining pending IO to the codec.
  base::RepeatingTimer io_timer_;

  // Backing strategy that we'll use to connect PictureBuffers to frames.
  scoped_ptr<BackingStrategy> strategy_;

  // Helper class that manages asynchronous OnFrameAvailable callbacks.
  class OnFrameAvailableHandler;
  scoped_refptr<OnFrameAvailableHandler> on_frame_available_handler_;

  // Time at which we last did useful work on io_timer_.
  base::TimeTicks most_recent_work_;

  // CDM related stuff.

  // Holds a ref-count to the CDM.
  scoped_refptr<media::MediaKeys> cdm_;

  // MediaDrmBridge requires registration/unregistration of the player, this
  // registration id is used for this.
  int cdm_registration_id_;

  // The MediaCrypto object is used in the MediaCodec.configure() in case of
  // an encrypted stream.
  media::MediaDrmBridge::JavaObjectPtr media_crypto_;

  // Index of the dequeued and filled buffer that we keep trying to enqueue.
  // Such buffer appears in MEDIA_CODEC_NO_KEY processing.
  int pending_input_buf_index_;

  // Monotonically increasing value that is used to prevent old, delayed errors
  // from being sent after a reset.
  int error_sequence_token_;

  // PostError will defer sending an error if and only if this is true.
  bool defer_errors_;

  // WeakPtrFactory for posting tasks back to |this|.
  base::WeakPtrFactory<AndroidVideoDecodeAccelerator> weak_this_factory_;

  friend class AndroidVideoDecodeAcceleratorTest;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_ANDROID_VIDEO_DECODE_ACCELERATOR_H_
