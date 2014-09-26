// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_VT_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_VT_VIDEO_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <map>
#include <queue>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "content/common/gpu/media/vt.h"
#include "media/filters/h264_parser.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context_cgl.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {

// VideoToolbox.framework implementation of the VideoDecodeAccelerator
// interface for Mac OS X (currently limited to 10.9+).
class VTVideoDecodeAccelerator
    : public media::VideoDecodeAccelerator,
      public base::NonThreadSafe {
 public:
  explicit VTVideoDecodeAccelerator(CGLContextObj cgl_context);
  virtual ~VTVideoDecodeAccelerator();

  // VideoDecodeAccelerator implementation.
  virtual bool Initialize(
      media::VideoCodecProfile profile,
      Client* client) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& pictures) OVERRIDE;
  virtual void ReusePictureBuffer(int32_t picture_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool CanDecodeOnIOThread() OVERRIDE;

  // Called by OutputThunk() when VideoToolbox finishes decoding a frame.
  void Output(
      int32_t bitstream_id,
      OSStatus status,
      CVImageBufferRef image_buffer);

 private:
  struct DecodedFrame {
    DecodedFrame(int32_t bitstream_id, CVImageBufferRef image_buffer);
    ~DecodedFrame();

    int32_t bitstream_id;
    base::ScopedCFTypeRef<CVImageBufferRef> image_buffer;
  };

  // Actions are the possible types of pending operations, which are queued
  // by Flush(), Reset(), and Destroy().
  enum Action {
    ACTION_FLUSH,
    ACTION_RESET,
    ACTION_DESTROY
  };

  // PendingActions contain the |bitstream_id| of a frame that, once decoded and
  // sent, a particular |action| should be completed at.
  struct PendingAction {
    PendingAction(Action action, int32_t bitstream_id);
    ~PendingAction();

    Action action;
    int32_t bitstream_id;
  };

  // Methods for interacting with VideoToolbox. Run on |decoder_thread_|.
  void ConfigureDecoder(
      const std::vector<const uint8_t*>& nalu_data_ptrs,
      const std::vector<size_t>& nalu_data_sizes);
  void DecodeTask(const media::BitstreamBuffer);
  void FlushTask();

  // Methods for interacting with |client_|. Run on |gpu_task_runner_|.
  void OutputTask(DecodedFrame frame);
  void SizeChangedTask(gfx::Size coded_size);

  // Send decoded frames up to and including |up_to_bitstream_id|, and return
  // the last sent |bitstream_id|.
  int32_t SendPictures(int32_t up_to_bitstream_id);

  // Since VideoToolbox has no reset feature (only flush), and the VDA API
  // allows Decode() and Flush() calls during a reset operation, it's possible
  // to have multiple pending actions at once. We handle the fully general case
  // of an arbitrary sequence of pending actions (in reality, there should
  // probably be at most one reset and one flush at a time).
  void QueueAction(Action action);

  // Process queued decoded frames, usually by sending them (unless there
  // is a pending ACTION_RESET or ACTION_DESTROY, in which case they are
  // dropped), completing queued actions along the way.
  void ProcessDecodedFrames();

  // Complete a particular action, by eg. calling NotifyFlushDone().
  // Warning: Deletes |this| if |action| is ACTION_DESTROY.
  void CompleteAction(Action action);

  // Complete all actions pending for a particular |bitstream_id|.
  // Warning: Do not call if there is a pending ACTION_DESTROY.
  void CompleteActions(int32_t bitstream_id);

  //
  // GPU thread state.
  //
  CGLContextObj cgl_context_;
  media::VideoDecodeAccelerator::Client* client_;
  gfx::Size texture_size_;
  std::queue<PendingAction> pending_actions_;
  std::queue<int32_t> pending_bitstream_ids_;

  // Texture IDs of pictures.
  // TODO(sandersd): A single map of structs holding picture data.
  std::map<int32_t, uint32_t> texture_ids_;

  // Pictures ready to be rendered to.
  std::queue<int32_t> available_picture_ids_;

  // Decoded frames ready to render.
  std::queue<DecodedFrame> decoded_frames_;

  // Image buffers kept alive while they are bound to pictures.
  std::map<int32_t, base::ScopedCFTypeRef<CVImageBufferRef>> picture_bindings_;

  //
  // Decoder thread state.
  //
  VTDecompressionOutputCallbackRecord callback_;
  base::ScopedCFTypeRef<CMFormatDescriptionRef> format_;
  base::ScopedCFTypeRef<VTDecompressionSessionRef> session_;
  media::H264Parser parser_;
  gfx::Size coded_size_;

  //
  // Shared state (set up and torn down on GPU thread).
  //
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  // This WeakPtrFactory does not need to be last as its pointers are bound to
  // the same thread it is destructed on (the GPU thread).
  base::WeakPtrFactory<VTVideoDecodeAccelerator> weak_this_factory_;

  // Declared last to ensure that all decoder thread tasks complete before any
  // state is destructed.
  base::Thread decoder_thread_;

  DISALLOW_COPY_AND_ASSIGN(VTVideoDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VT_VIDEO_DECODE_ACCELERATOR_H_
