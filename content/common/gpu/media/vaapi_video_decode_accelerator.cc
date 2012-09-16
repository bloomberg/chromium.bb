// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "content/public/common/content_switches.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/media/vaapi_video_decode_accelerator.h"
#include "media/base/bind_to_loop.h"
#include "media/video/picture.h"
#include "third_party/libva/va/va.h"
#include "ui/gl/gl_bindings.h"

#define RETURN_AND_NOTIFY_ON_FAILURE(result, log, error_code, ret)  \
  do {                                                              \
    if (!(result)) {                                                \
      DVLOG(1) << log;                                              \
      NotifyError(error_code);                                      \
      return ret;                                                   \
    }                                                               \
  } while (0)

using content::VaapiH264Decoder;

VaapiVideoDecodeAccelerator::InputBuffer::InputBuffer() : id(0), size(0) {
}

VaapiVideoDecodeAccelerator::InputBuffer::~InputBuffer() {
}

void VaapiVideoDecodeAccelerator::NotifyError(Error error) {
  if (message_loop_ != MessageLoop::current()) {
    DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &VaapiVideoDecodeAccelerator::NotifyError, weak_this_, error));
    return;
  }

  DVLOG(1) << "Notifying of error " << error;

  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.InvalidateWeakPtrs();
  }
  Cleanup();
}

VaapiVideoDecodeAccelerator::VaapiVideoDecodeAccelerator(
    Display* x_display, GLXContext glx_context,
    Client* client,
    const base::Callback<bool(void)>& make_context_current)
    : x_display_(x_display),
      glx_context_(glx_context),
      make_context_current_(make_context_current),
      state_(kUninitialized),
      input_ready_(&lock_),
      output_ready_(&lock_),
      message_loop_(MessageLoop::current()),
      weak_this_(base::AsWeakPtr(this)),
      client_ptr_factory_(client),
      client_(client_ptr_factory_.GetWeakPtr()),
      decoder_thread_("VaapiDecoderThread"),
      num_frames_at_client_(0),
      num_stream_bufs_at_decoder_(0) {
  DCHECK(client);
  static bool vaapi_functions_initialized = PostSandboxInitialization();
  RETURN_AND_NOTIFY_ON_FAILURE(vaapi_functions_initialized,
                               "Failed to initialize VAAPI libs",
                               PLATFORM_FAILURE, );
}

VaapiVideoDecodeAccelerator::~VaapiVideoDecodeAccelerator() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
}

bool VaapiVideoDecodeAccelerator::Initialize(
    media::VideoCodecProfile profile) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kUninitialized);
  DVLOG(2) << "Initializing VAVDA, profile: " << profile;

  bool res = decoder_.Initialize(
      profile, x_display_, glx_context_, make_context_current_,
      media::BindToLoop(message_loop_->message_loop_proxy(), base::Bind(
          &VaapiVideoDecodeAccelerator::NotifyPictureReady, weak_this_)),
      media::BindToLoop(message_loop_->message_loop_proxy(), base::Bind(
          &VaapiVideoDecodeAccelerator::Sync, weak_this_)));
  if (!res) {
    DVLOG(1) << "Failed initializing decoder";
    return false;
  }

  CHECK(decoder_thread_.Start());

  state_ = kInitialized;

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &Client::NotifyInitializeDone, client_));
  return true;
}

void VaapiVideoDecodeAccelerator::Sync(int32 output_id) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  TRACE_EVENT1("Video Decoder", "VAVDA::Sync", "output_id", output_id);

  // Handle Destroy() arriving while pictures are queued for output.
  if (!client_)
    return;

  // Sync the contents of the texture.
  RETURN_AND_NOTIFY_ON_FAILURE(decoder_.PutPicToTexture(output_id),
                               "Failed putting picture to texture",
                               PLATFORM_FAILURE, );
}

void VaapiVideoDecodeAccelerator::NotifyPictureReady(int32 input_id,
                                                     int32 output_id) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  TRACE_EVENT2("Video Decoder", "VAVDA::NotifyPictureReady",
               "input_id", input_id, "output_id", output_id);

  // Handle Destroy() arriving while pictures are queued for output.
  if (!client_)
    return;

  ++num_frames_at_client_;
  TRACE_COUNTER1("Video Decoder", "Textures at client", num_frames_at_client_);

  // Notify the client a picture is ready to be displayed.
  DVLOG(4) << "Notifying output picture id " << output_id
           << " for input "<< input_id << " is ready";
  client_->PictureReady(media::Picture(output_id, input_id));
}

void VaapiVideoDecodeAccelerator::MapAndQueueNewInputBuffer(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  TRACE_EVENT1("Video Decoder", "MapAndQueueNewInputBuffer", "input_id",
      bitstream_buffer.id());

  DVLOG(4) << "Mapping new input buffer id: " << bitstream_buffer.id()
           << " size: " << (int)bitstream_buffer.size();

  scoped_ptr<base::SharedMemory> shm(
      new base::SharedMemory(bitstream_buffer.handle(), true));
  RETURN_AND_NOTIFY_ON_FAILURE(shm->Map(bitstream_buffer.size()),
                              "Failed to map input buffer", UNREADABLE_INPUT,);

  base::AutoLock auto_lock(lock_);

  // Set up a new input buffer and queue it for later.
  linked_ptr<InputBuffer> input_buffer(new InputBuffer());
  input_buffer->shm.reset(shm.release());
  input_buffer->id = bitstream_buffer.id();
  input_buffer->size = bitstream_buffer.size();

  ++num_stream_bufs_at_decoder_;
  TRACE_COUNTER1("Video Decoder", "Stream buffers at decoder",
                 num_stream_bufs_at_decoder_);

  input_buffers_.push(input_buffer);
  input_ready_.Signal();
}

void VaapiVideoDecodeAccelerator::InitialDecodeTask() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  base::AutoLock auto_lock(lock_);

  // Try to initialize or resume playback after reset.
  while (GetInputBuffer_Locked()) {
    DCHECK(curr_input_buffer_.get());

    // Since multiple Decode()'s can be in flight at once, it's possible that a
    // Decode() that seemed like an initial one is actually later in the stream
    // and we're already kDecoding.  Let the normal DecodeTask take over in that
    // case.
    if (state_ != kInitialized && state_ != kIdle)
      return;

    VaapiH264Decoder::DecResult res =
        decoder_.DecodeInitial(curr_input_buffer_->id);
    switch (res) {
      case VaapiH264Decoder::kReadyToDecode:
        if (state_ == kInitialized) {
          state_ = kPicturesRequested;
          int num_pics = decoder_.GetRequiredNumOfPictures();
          gfx::Size size(decoder_.pic_width(), decoder_.pic_height());
          DVLOG(1) << "Requesting " << num_pics << " pictures of size: "
                   << size.width() << "x" << size.height();
          message_loop_->PostTask(FROM_HERE, base::Bind(
              &Client::ProvidePictureBuffers, client_,
              num_pics, size, GL_TEXTURE_2D));
        } else {
          DCHECK_EQ(state_, kIdle);
          state_ = kDecoding;
          decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
              &VaapiVideoDecodeAccelerator::DecodeTask,
              base::Unretained(this)));
        }
        return;

      case VaapiH264Decoder::kNeedMoreStreamData:
        ReturnCurrInputBuffer_Locked();
        break;

      case VaapiH264Decoder::kNoOutputAvailable:
        if (state_ == kIdle)  {
          // No more output buffers in the decoder, try getting more or go to
          // sleep waiting for them.
          GetOutputBuffers_Locked();
          return;
        }
        // else fallthrough
      case VaapiH264Decoder::kDecodeError:
        RETURN_AND_NOTIFY_ON_FAILURE(false, "Error in decoding",
                                     PLATFORM_FAILURE, );

      default:
        RETURN_AND_NOTIFY_ON_FAILURE(false,
                                     "Unexpected result from decoder: " << res,
                                     PLATFORM_FAILURE, );
    }
  }
}

bool VaapiVideoDecodeAccelerator::GetInputBuffer_Locked() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  lock_.AssertAcquired();

  if (curr_input_buffer_.get())
    return true;

  // Will only wait if it is expected that in current state new buffers will
  // be queued from the client via Decode(). The state can change during wait.
  while (input_buffers_.empty() &&
         (state_ == kDecoding || state_ == kInitialized || state_ == kIdle)) {
    input_ready_.Wait();
  }

  // We could have got woken up in a different state or never got to sleep
  // due to current state; check for that.
  switch (state_) {
    case kFlushing:
      // Here we are only interested in finishing up decoding buffers that are
      // already queued up. Otherwise will stop decoding.
      if (input_buffers_.empty())
        return false;
      // else fallthrough
    case kDecoding:
    case kInitialized:
    case kIdle:
      DCHECK(!input_buffers_.empty());

      curr_input_buffer_ = input_buffers_.front();
      input_buffers_.pop();

      DVLOG(4) << "New current bitstream buffer, id: "
               << curr_input_buffer_->id
               << " size: " << curr_input_buffer_->size;

      decoder_.SetStream(
          static_cast<uint8*>(curr_input_buffer_->shm->memory()),
          curr_input_buffer_->size);
      return true;

    default:
      // We got woken up due to being destroyed/reset, ignore any already
      // queued inputs.
      return false;
  }
}

void VaapiVideoDecodeAccelerator::ReturnCurrInputBuffer_Locked() {
  lock_.AssertAcquired();
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK(curr_input_buffer_.get());

  int32 id = curr_input_buffer_->id;
  curr_input_buffer_.reset();
  DVLOG(4) << "End of input buffer " << id;
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &Client::NotifyEndOfBitstreamBuffer, client_, id));

  --num_stream_bufs_at_decoder_;
  TRACE_COUNTER1("Video Decoder", "Stream buffers at decoder",
                 num_stream_bufs_at_decoder_);
}

bool VaapiVideoDecodeAccelerator::GetOutputBuffers_Locked() {
  lock_.AssertAcquired();
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  while (output_buffers_.empty() &&
         (state_ == kDecoding || state_ == kFlushing || state_ == kIdle)) {
    output_ready_.Wait();
  }

  if (state_ != kDecoding && state_ != kFlushing && state_ != kIdle)
    return false;

  while (!output_buffers_.empty()) {
    decoder_.ReusePictureBuffer(output_buffers_.front());
    output_buffers_.pop();
  }

  return true;
}

void VaapiVideoDecodeAccelerator::DecodeTask() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "VAVDA::DecodeTask");
  base::AutoLock auto_lock(lock_);

  // Main decode task.
  DVLOG(4) << "Decode task";

  // Try to decode what stream data is (still) in the decoder until we run out
  // of it.
  while (GetInputBuffer_Locked()) {
    DCHECK(curr_input_buffer_.get());

    VaapiH264Decoder::DecResult res;
    {
      base::AutoUnlock auto_unlock(lock_);
      res = decoder_.DecodeOneFrame(curr_input_buffer_->id);
    }

    switch (res) {
      case VaapiH264Decoder::kNeedMoreStreamData:
        ReturnCurrInputBuffer_Locked();
        break;

      case VaapiH264Decoder::kDecodedFrame:
        // May still have more stream data, continue decoding.
        break;

      case VaapiH264Decoder::kNoOutputAvailable:
        // No more output buffers in the decoder, try getting more or go to
        // sleep waiting for them.
        if (!GetOutputBuffers_Locked())
          return;
        break;

      case VaapiH264Decoder::kDecodeError:
        RETURN_AND_NOTIFY_ON_FAILURE(false, "Error decoding stream",
                                     PLATFORM_FAILURE, );
        return;

      default:
        RETURN_AND_NOTIFY_ON_FAILURE(
            false, "Unexpected result from the decoder: " << res,
            PLATFORM_FAILURE, );
        return;
    }
  }
}

void VaapiVideoDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  TRACE_EVENT1("Video Decoder", "VAVDA::Decode", "Buffer id",
               bitstream_buffer.id());

  // We got a new input buffer from the client, map it and queue for later use.
  MapAndQueueNewInputBuffer(bitstream_buffer);

  base::AutoLock auto_lock(lock_);
  switch (state_) {
    case kInitialized:
      // Initial decode to get the required size of output buffers.
      decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
          &VaapiVideoDecodeAccelerator::InitialDecodeTask,
          base::Unretained(this)));
      break;

    case kPicturesRequested:
      // Waiting for pictures, return.
      break;

    case kDecoding:
      break;

    case kIdle:
      // Need to get decoder into suitable stream location to resume.
      decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
          &VaapiVideoDecodeAccelerator::InitialDecodeTask,
          base::Unretained(this)));
      break;

    default:
      RETURN_AND_NOTIFY_ON_FAILURE(false,
          "Decode request from client in invalid state: " << state_,
          PLATFORM_FAILURE, );
      return;
  }
}

void VaapiVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPicturesRequested);

  for (size_t i = 0; i < buffers.size(); ++i) {
    DVLOG(2) << "Assigning picture id " << buffers[i].id()
             << " to texture id " << buffers[i].texture_id();

    bool res = decoder_.AssignPictureBuffer(buffers[i].id(),
                                            buffers[i].texture_id());
    RETURN_AND_NOTIFY_ON_FAILURE(
        res, "Failed assigning picture buffer id: " << buffers[i].id() <<
        ", texture id: " << buffers[i].texture_id(), PLATFORM_FAILURE, );
  }

  state_ = kDecoding;
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::DecodeTask, base::Unretained(this)));
}

void VaapiVideoDecodeAccelerator::ReusePictureBuffer(int32 picture_buffer_id) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  TRACE_EVENT1("Video Decoder", "VAVDA::ReusePictureBuffer", "Picture id",
               picture_buffer_id);

  --num_frames_at_client_;
  TRACE_COUNTER1("Video Decoder", "Textures at client", num_frames_at_client_);

  base::AutoLock auto_lock(lock_);
  output_buffers_.push(picture_buffer_id);
  output_ready_.Signal();
}

void VaapiVideoDecodeAccelerator::FlushTask() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DVLOG(1) << "Flush task";

  // First flush all the pictures that haven't been outputted, notifying the
  // client to output them.
  bool res = decoder_.Flush();
  RETURN_AND_NOTIFY_ON_FAILURE(res, "Failed flushing the decoder.",
                               PLATFORM_FAILURE, );

  // Put the decoder in idle state, ready to resume.
  decoder_.Reset();

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::FinishFlush, weak_this_));
}

void VaapiVideoDecodeAccelerator::Flush() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DVLOG(1) << "Got flush request";

  base::AutoLock auto_lock(lock_);
  state_ = kFlushing;
  // Queue a flush task after all existing decoding tasks to clean up.
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::FlushTask, base::Unretained(this)));

  input_ready_.Signal();
  output_ready_.Signal();
}

void VaapiVideoDecodeAccelerator::FinishFlush() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  base::AutoLock auto_lock(lock_);
  if (state_ != kFlushing) {
    DCHECK_EQ(state_, kDestroying);
    return;  // We could've gotten destroyed already.
  }

  state_ = kIdle;

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &Client::NotifyFlushDone, client_));

  DVLOG(1) << "Flush finished";
}

void VaapiVideoDecodeAccelerator::ResetTask() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  // All the decoding tasks from before the reset request from client are done
  // by now, as this task was scheduled after them and client is expected not
  // to call Decode() after Reset() and before NotifyResetDone.
  decoder_.Reset();

  base::AutoLock auto_lock(lock_);

  // Return current input buffer, if present.
  if (curr_input_buffer_.get())
    ReturnCurrInputBuffer_Locked();

  // And let client know that we are done with reset.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::FinishReset, weak_this_));
}

void VaapiVideoDecodeAccelerator::Reset() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DVLOG(1) << "Got reset request";

  // This will make any new decode tasks exit early.
  base::AutoLock auto_lock(lock_);
  state_ = kResetting;

  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::ResetTask, base::Unretained(this)));

  input_ready_.Signal();
  output_ready_.Signal();
}

void VaapiVideoDecodeAccelerator::FinishReset() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  base::AutoLock auto_lock(lock_);
  if (state_ != kResetting) {
    DCHECK(state_ == kDestroying || state_ == kUninitialized) << state_;
    return;  // We could've gotten destroyed already.
  }

  // Drop all remaining input buffers, if present.
  while (!input_buffers_.empty()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &Client::NotifyEndOfBitstreamBuffer, client_,
        input_buffers_.front()->id));
    input_buffers_.pop();
  }

  state_ = kIdle;
  num_stream_bufs_at_decoder_ = 0;

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &Client::NotifyResetDone, client_));

  DVLOG(1) << "Reset finished";
}

void VaapiVideoDecodeAccelerator::Cleanup() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (state_ == kUninitialized || state_ == kDestroying)
    return;

  DVLOG(1) << "Destroying VAVDA";
  base::AutoLock auto_lock(lock_);
  state_ = kDestroying;

  client_ptr_factory_.InvalidateWeakPtrs();

  {
    base::AutoUnlock auto_unlock(lock_);
    // Post a dummy task to the decoder_thread_ to ensure it is drained.
    base::WaitableEvent waiter(false, false);
    decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
        &base::WaitableEvent::Signal, base::Unretained(&waiter)));
    input_ready_.Signal();
    output_ready_.Signal();
    waiter.Wait();
    decoder_thread_.Stop();
  }

  decoder_.Destroy();
  state_ = kUninitialized;
}

void VaapiVideoDecodeAccelerator::Destroy() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  Cleanup();
  delete this;
}

// static
void VaapiVideoDecodeAccelerator::PreSandboxInitialization() {
  VaapiH264Decoder::PreSandboxInitialization();
}

// static
bool VaapiVideoDecodeAccelerator::PostSandboxInitialization() {
  return VaapiH264Decoder::PostSandboxInitialization();
}
