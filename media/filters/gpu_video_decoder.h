// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_GPU_VIDEO_DECODER_H_
#define MEDIA_FILTERS_GPU_VIDEO_DECODER_H_

#include <list>
#include <map>
#include <utility>
#include <vector>

#include "media/base/pipeline_status.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder.h"
#include "media/video/video_decode_accelerator.h"

class MessageLoop;
template <class T> class scoped_refptr;
namespace base {
class MessageLoopProxy;
class SharedMemory;
}

namespace media {

class DecoderBuffer;

// GPU-accelerated video decoder implementation.  Relies on
// AcceleratedVideoDecoderMsg_Decode and friends.
// All methods internally trampoline to the |message_loop| passed to the ctor.
class MEDIA_EXPORT GpuVideoDecoder
    : public VideoDecoder,
      public VideoDecodeAccelerator::Client {
 public:
  // Helper interface for specifying factories needed to instantiate a
  // GpuVideoDecoder.
  class MEDIA_EXPORT Factories : public base::RefCountedThreadSafe<Factories> {
   public:
    // Caller owns returned pointer.
    virtual VideoDecodeAccelerator* CreateVideoDecodeAccelerator(
        VideoCodecProfile, VideoDecodeAccelerator::Client*) = 0;

    // Allocate & delete native textures.
    virtual bool CreateTextures(int32 count, const gfx::Size& size,
                                std::vector<uint32>* texture_ids,
                                uint32 texture_target) = 0;
    virtual void DeleteTexture(uint32 texture_id) = 0;

    // Read pixels from a native texture and store into |*pixels| as RGBA.
    virtual void ReadPixels(uint32 texture_id, uint32 texture_target,
                            const gfx::Size& size, void* pixels) = 0;

    // Allocate & return a shared memory segment.  Caller is responsible for
    // Close()ing the returned pointer.
    virtual base::SharedMemory* CreateSharedMemory(size_t size) = 0;

    // Returns the message loop the VideoDecodeAccelerator runs on.
    virtual scoped_refptr<base::MessageLoopProxy> GetMessageLoop() = 0;

   protected:
    friend class base::RefCountedThreadSafe<Factories>;
    virtual ~Factories();
  };

  GpuVideoDecoder(const scoped_refptr<base::MessageLoopProxy>& message_loop,
                  const scoped_refptr<Factories>& factories);

  // VideoDecoder implementation.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;
  virtual bool HasAlpha() const OVERRIDE;
  virtual bool HasOutputFrameAvailable() const OVERRIDE;

  // VideoDecodeAccelerator::Client implementation.
  virtual void NotifyInitializeDone() OVERRIDE;
  virtual void ProvidePictureBuffers(uint32 count,
                                     const gfx::Size& size,
                                     uint32 texture_target) OVERRIDE;
  virtual void DismissPictureBuffer(int32 id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 id) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyResetDone() OVERRIDE;
  virtual void NotifyError(media::VideoDecodeAccelerator::Error error) OVERRIDE;

 protected:
  virtual ~GpuVideoDecoder();

 private:
  enum State {
    kNormal,
    // Avoid the use of "flush" in these enums because the term is overloaded:
    // Filter::Flush() means drop pending data on the floor, but
    // VideoDecodeAccelerator::Flush() means drain pending data (Filter::Flush()
    // actually corresponds to VideoDecodeAccelerator::Reset(), confusingly
    // enough).
    kDrainingDecoder,
    kDecoderDrained,
  };

  // If no demuxer read is in flight and no bitstream buffers are in the
  // decoder, kick some off demuxing/decoding.
  void EnsureDemuxOrDecode();

  // Return true if more decode work can be piled on to the VDA.
  bool CanMoreDecodeWorkBeDone();

  // Callback to pass to demuxer_stream_->Read() for receiving encoded bits.
  void RequestBufferDecode(DemuxerStream::Status status,
                           const scoped_refptr<DecoderBuffer>& buffer);

  // Enqueue a frame for later delivery (or drop it on the floor if a
  // vda->Reset() is in progress) and trigger out-of-line delivery of the oldest
  // ready frame to the client if there is a pending read.  A NULL |frame|
  // merely triggers delivery, and requires the ready_video_frames_ queue not be
  // empty.
  void EnqueueFrameAndTriggerFrameDelivery(
      const scoped_refptr<VideoFrame>& frame);

  // Indicate the picturebuffer can be reused by the decoder.
  void ReusePictureBuffer(int64 picture_buffer_id);

  void RecordBufferData(
      const BitstreamBuffer& bitstream_buffer, const DecoderBuffer& buffer);
  void GetBufferData(int32 id, base::TimeDelta* timetamp,
                     gfx::Rect* visible_rect, gfx::Size* natural_size);

  // Set |vda_| and |weak_vda_| on the VDA thread (in practice the render
  // thread).
  void SetVDA(VideoDecodeAccelerator* vda);

  // Call VDA::Destroy() on |vda_loop_proxy_| ensuring that |this| outlives the
  // Destroy() call.
  void DestroyVDA();

  // A shared memory segment and its allocated size.
  struct SHMBuffer {
    SHMBuffer(base::SharedMemory* m, size_t s);
    ~SHMBuffer();
    base::SharedMemory* shm;
    size_t size;
  };

  // Request a shared-memory segment of at least |min_size| bytes.  Will
  // allocate as necessary.  Caller does not own returned pointer.
  SHMBuffer* GetSHM(size_t min_size);

  // Return a shared-memory segment to the available pool.
  void PutSHM(SHMBuffer* shm_buffer);

  void DestroyTextures();

  StatisticsCB statistics_cb_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<DemuxerStream> demuxer_stream_;

  // MessageLoop on which to fire callbacks and trampoline calls to this class
  // if they arrive on other loops.
  scoped_refptr<base::MessageLoopProxy> gvd_loop_proxy_;

  // Message loop on which to makes all calls to vda_.  (beware this loop may be
  // paused during the Pause/Flush/Stop dance PipelineImpl::Stop() goes
  // through).
  scoped_refptr<base::MessageLoopProxy> vda_loop_proxy_;

  scoped_refptr<Factories> factories_;

  // Populated during Initialize() via SetVDA() (on success) and unchanged
  // until an error occurs
  scoped_ptr<VideoDecodeAccelerator> vda_;
  // Used to post tasks from the GVD thread to the VDA thread safely.
  base::WeakPtr<VideoDecodeAccelerator> weak_vda_;

  // Callbacks that are !is_null() only during their respective operation being
  // asynchronously executed.
  ReadCB pending_read_cb_;
  base::Closure pending_reset_cb_;

  State state_;

  // Is a demuxer read in flight?
  bool demuxer_read_in_progress_;

  // Shared-memory buffer pool.  Since allocating SHM segments requires a
  // round-trip to the browser process, we keep allocation out of the
  // steady-state of the decoder.
  std::vector<SHMBuffer*> available_shm_segments_;

  // Book-keeping variables.
  struct BufferPair {
    BufferPair(SHMBuffer* s, const scoped_refptr<DecoderBuffer>& b);
    ~BufferPair();
    SHMBuffer* shm_buffer;
    scoped_refptr<DecoderBuffer> buffer;
  };
  std::map<int32, BufferPair> bitstream_buffers_in_decoder_;
  std::map<int32, PictureBuffer> picture_buffers_in_decoder_;

  // The texture target used for decoded pictures.
  uint32 decoder_texture_target_;

  struct BufferData {
    BufferData(int32 bbid, base::TimeDelta ts, const gfx::Rect& visible_rect,
               const gfx::Size& natural_size);
    ~BufferData();
    int32 bitstream_buffer_id;
    base::TimeDelta timestamp;
    gfx::Rect visible_rect;
    gfx::Size natural_size;
  };
  std::list<BufferData> input_buffer_data_;

  // picture_buffer_id and the frame wrapping the corresponding Picture, for
  // frames that have been decoded but haven't been requested by a Read() yet.
  std::list<scoped_refptr<VideoFrame> > ready_video_frames_;
  int32 next_picture_buffer_id_;
  int32 next_bitstream_buffer_id_;

  // Indicates decoding error occurred.
  bool error_occured_;

  // Set during ProvidePictureBuffers(), used for checking and implementing
  // HasAvailableOutputFrames().
  int available_pictures_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_GPU_VIDEO_DECODER_H_
