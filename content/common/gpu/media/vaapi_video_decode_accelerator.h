// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of VideoDecoderAccelerator
// that utilizes hardware video decoder present on Intel CPUs.

#ifndef CONTENT_COMMON_GPU_MEDIA_VAAPI_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_VAAPI_VIDEO_DECODE_ACCELERATOR_H_

#include <GL/glx.h>

#include <queue>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "content/common/gpu/media/vaapi_h264_decoder.h"
#include "media/base/bitstream_buffer.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"

// Class to provide video decode acceleration for Intel systems with hardware
// support for it, and on which libva is available.
// Decoding tasks are performed in a separate decoding thread.
class VaapiVideoDecodeAccelerator : public media::VideoDecodeAccelerator {
 public:
  VaapiVideoDecodeAccelerator(Client* client);

  // media::VideoDecodeAccelerator implementation.
  virtual bool Initialize(media::VideoCodecProfile profile) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void Destroy() OVERRIDE;

  // Used by user of this class to pass X/GLX state.
  void SetGlxState(Display* x_display, GLXContext glx_context);

 private:
  virtual ~VaapiVideoDecodeAccelerator();

  void NotifyInitializeDone();

  // Notify the client that the input buffer has been consumed.
  void NotifyInputBufferRead(int input_buffer_id);

  // Ensure data has been synced with the output texture and notify
  // the client it is ready for displaying.
  void SyncAndNotifyPictureReady(int32 input_id, int32 output_id);

  // Posted by the decoder thread to notify VAVDA that the decoder has
  // initially parsed the stream and is ready to decode. If the pictures have
  // not yet been requested, it will request the client to provide |num_pics|
  // textures of given |size| and wait for them, otherwise will post
  // a DecodeTask directly.
  void ReadyToDecode(int num_pics, const gfx::Size& size);

  // Notify the client that an error has occurred and decoding cannot continue.
  void NotifyError(Error error);

  // Map the received input buffer into this process' address space and
  // queue it for decode.
  void MapAndQueueNewInputBuffer(
      const media::BitstreamBuffer& bitstream_buffer);

  // Get a new input buffer from the queue and set it up in decoder. This will
  // sleep if no input buffers are available. Return true if a new buffer has
  // been set up, false if an early exit has been requested (due to initiated
  // reset/flush/destroy).
  bool GetInputBuffer();

  // Signal the client that the current buffer has been read and can be
  // returned. Will also release the mapping.
  void ReturnCurrInputBuffer();

  // Get and set up one or more output buffers in the decoder. This will sleep
  // if no buffers are available. Return true if buffers have been set up or
  // false if an early exit has been requested (due to initiated
  // reset/flush/destroy).
  bool GetOutputBuffers();

  // Initial decode task: get the decoder to the point in the stream from which
  // it can start/continue decoding. Does not require output buffers and does
  // not produce output frames. Called either when starting with a new stream
  // or when playback is to be resumed following a seek.
  void InitialDecodeTask();

  // Decoding task. Will continue decoding given input buffers and sleep
  // waiting for input/output as needed. Will exit if a reset/flush/destroy
  // is requested.
  void DecodeTask();

  // Scheduled after receiving a flush request and executed after the current
  // decoding task finishes decoding pending inputs. Makes the decoder return
  // all remaining output pictures and puts it in an idle state, ready
  // to resume if needed and schedules a FinishFlush.
  void FlushTask();

  // Scheduled by the FlushTask after decoder is flushed to put VAVDA into idle
  // state and notify the client that flushing has been finished.
  void FinishFlush();

  // Scheduled after receiving a reset request and executed after the current
  // decoding task finishes decoding the current frame. Puts the decoder into
  // an idle state, ready to resume if needed, discarding decoded but not yet
  // outputted pictures (decoder keeps ownership of their associated picture
  // buffers). Schedules a FinishReset afterwards.
  void ResetTask();

  // Scheduled by ResetTask after it's done putting VAVDA into an idle state.
  // Drops remaining input buffers and notifies the client that reset has been
  // finished.
  void FinishReset();

  // Scheduled on the decoder thread after receiving a Destroy() call from the
  // client, executed after the current decoding task finishes decoding the
  // current frame, ignoring any remaining inputs. Cleans up the decoder and
  // frees all resources.
  void DestroyTask();

  // Scheduled by DestroyTask after it's done destroying the decoder, puts
  // VAVDA into an uninitialized state.
  void FinishDestroy();

  // Client-provided X/GLX state.
  Display* x_display_;
  GLXContext glx_context_;

  // VAVDA state.
  enum State {
    // Initialize() not called yet or failed.
    kUninitialized,
    // Initialize() succeeded, no initial decode and no pictures requested.
    kInitialized,
    // Initial decode finished, requested pictures and waiting for them.
    kPicturesRequested,
    // Everything initialized, pictures received and assigned, in decoding.
    kDecoding,
    // Resetting, waiting for decoder to finish current task and cleanup.
    kResetting,
    // Flushing, waiting for decoder to finish current task and cleanup.
    kFlushing,
    // Idle, decoder in state ready to resume decoding.
    kIdle,
    // Destroying, waiting for the decoder to finish current task.
    kDestroying,
  };

  State state_;

  // Protects input and output buffer queues and state_.
  base::Lock lock_;

  // An input buffer awaiting consumption, provided by the client.
  struct InputBuffer {
    InputBuffer();
    ~InputBuffer();

    int32 id;
    size_t size;
    scoped_ptr<base::SharedMemory> shm;
  };

  // Queue for incoming input buffers.
  typedef std::queue<linked_ptr<InputBuffer> > InputBuffers;
  InputBuffers input_buffers_;
  // Signalled when input buffers are queued onto the input_buffers_ queue.
  base::ConditionVariable input_ready_;

  // Current input buffer at decoder.
  linked_ptr<InputBuffer> curr_input_buffer_;

  // Queue for incoming input buffers.
  typedef std::queue<int32> OutputBuffers;
  OutputBuffers output_buffers_;
  // Signalled when output buffers are queued onto the output_buffers_ queue.
  base::ConditionVariable output_ready_;

  // ChildThread's message loop
  MessageLoop* message_loop_;

  // To expose client callbacks from VideoDecodeAccelerator.
  // NOTE: all calls to this object *MUST* be executed on message_loop_.
  Client* client_;

  base::Thread decoder_thread_;
  content::VaapiH264Decoder decoder_;

  // Callback passed to the decoder, which it will use to signal readiness
  // of an output picture to be displayed.
  void OutputPicCallback(int32 input_id, int32 output_id);

  DISALLOW_COPY_AND_ASSIGN(VaapiVideoDecodeAccelerator);
};

#endif  // CONTENT_COMMON_GPU_MEDIA_VAAPI_VIDEO_DECODE_ACCELERATOR_H_

