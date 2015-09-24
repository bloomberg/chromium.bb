// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_ANDROID_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_ANDROID_VIDEO_DECODE_ACCELERATOR_H_

#include <list>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/common/gpu/media/android_video_decode_accelerator_state_provider.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "media/base/android/media_codec_bridge.h"
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
      public AndroidVideoDecodeAcceleratorStateProvider {
 public:
  // A BackingStrategy is responsible for making a PictureBuffer's texture
  // contain the image that a MediaCodec decoder buffer tells it to.
  class BackingStrategy {
   public:
    virtual ~BackingStrategy() {}

    // Notify about the state provider.
    virtual void SetStateProvider(
        AndroidVideoDecodeAcceleratorStateProvider*) = 0;

    // Called before the AVDA does any Destroy() work.  This will be
    // the last call that the BackingStrategy receives.
    virtual void Cleanup() = 0;

    // Return the number of picture buffers that we can support.
    virtual uint32 GetNumPictureBuffers() const = 0;

    // Return the GL texture target that the PictureBuffer textures use.
    virtual uint32 GetTextureTarget() const = 0;

    // Use the provided PictureBuffer to hold the current surface.
    virtual void AssignCurrentSurfaceToPictureBuffer(
        int32 codec_buffer_index,
        const media::PictureBuffer&) = 0;
  };

  AndroidVideoDecodeAccelerator(
      const base::WeakPtr<gpu::gles2::GLES2Decoder> decoder,
      const base::Callback<bool(void)>& make_context_current,
      scoped_ptr<BackingStrategy> strategy);

  ~AndroidVideoDecodeAccelerator() override;

  // Does not take ownership of |client| which must outlive |*this|.
  bool Initialize(media::VideoCodecProfile profile, Client* client) override;
  void Decode(const media::BitstreamBuffer& bitstream_buffer) override;
  void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) override;
  void ReusePictureBuffer(int32 picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;
  bool CanDecodeOnIOThread() override;

  // AndroidVideoDecodeStateProvider
  const gfx::Size& GetSize() const override;
  const base::ThreadChecker& ThreadChecker() const override;
  gfx::SurfaceTexture* GetSurfaceTexture() const override;
  uint32 GetSurfaceTextureId() const override;
  gpu::gles2::GLES2Decoder* GetGlDecoder() const override;
  media::VideoCodecBridge* GetMediaCodec() override;
  void PostError(const ::tracked_objects::Location& from_here,
                 media::VideoDecodeAccelerator::Error error) override;

  static media::VideoDecodeAccelerator::SupportedProfiles
      GetSupportedProfiles();

 private:
  enum State {
    NO_ERROR,
    ERROR,
  };

  static const base::TimeDelta kDecodePollDelay;

  // Configures |media_codec_| with the given codec parameters from the client.
  bool ConfigureMediaCodec();

  // Sends the current picture on the surface to the client.
  void SendCurrentSurfaceToClient(int32 codec_buffer_index, int32 bitstream_id);

  // Does pending IO tasks if any. Once this is called, it polls |media_codec_|
  // until it finishes pending tasks. For the polling, |kDecodePollDelay| is
  // used.
  void DoIOTask();

  // Feeds input data to |media_codec_|. This checks
  // |pending_bitstream_buffers_| and queues a buffer to |media_codec_|.
  void QueueInput();

  // Dequeues output from |media_codec_| and feeds the decoded frame to the
  // client.
  void DequeueOutput();

  // Requests picture buffers from the client.
  void RequestPictureBuffers();

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
  void NotifyError(media::VideoDecodeAccelerator::Error error);

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // To expose client callbacks from VideoDecodeAccelerator.
  Client* client_;

  // Callback to set the correct gl context.
  base::Callback<bool(void)> make_context_current_;

  // Codec type. Used when we configure media codec.
  media::VideoCodec codec_;

  // The current state of this class. For now, this is used only for setting
  // error state.
  State state_;

  // This map maintains the picture buffers passed to the client for decoding.
  // The key is the picture buffer id.
  typedef std::map<int32, media::PictureBuffer> OutputBufferMap;
  OutputBufferMap output_picture_buffers_;

  // This keeps the free picture buffer ids which can be used for sending
  // decoded frames to the client.
  std::queue<int32> free_picture_ids_;

  // Picture buffer ids which have been dismissed and not yet re-assigned.  Used
  // to ignore ReusePictureBuffer calls that were in flight when the
  // DismissPictureBuffer call was made.
  std::set<int32> dismissed_picture_ids_;

  // The low-level decoder which Android SDK provides.
  scoped_ptr<media::VideoCodecBridge> media_codec_;

  // A container of texture. Used to set a texture to |media_codec_|.
  scoped_refptr<gfx::SurfaceTexture> surface_texture_;

  // The texture id which is set to |surface_texture_|.
  uint32 surface_texture_id_;

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
  std::map<base::TimeDelta, int32> bitstream_buffers_in_decoder_;

  // Keeps track of bitstream ids notified to the client with
  // NotifyEndOfBitstreamBuffer() before getting output from the bitstream.
  std::list<int32> bitstreams_notified_in_advance_;

  // Owner of the GL context. Used to restore the context state.
  base::WeakPtr<gpu::gles2::GLES2Decoder> gl_decoder_;

  // Repeating timer responsible for draining pending IO to the codec.
  base::RepeatingTimer io_timer_;

  // Backing strategy that we'll use to connect PictureBuffers to frames.
  scoped_ptr<BackingStrategy> strategy_;

  // WeakPtrFactory for posting tasks back to |this|.
  base::WeakPtrFactory<AndroidVideoDecodeAccelerator> weak_this_factory_;

  friend class AndroidVideoDecodeAcceleratorTest;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_ANDROID_VIDEO_DECODE_ACCELERATOR_H_
