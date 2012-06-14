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
#include "media/video/picture.h"
#include "third_party/libva/va/va.h"
#include "ui/gl/gl_bindings.h"

#define RETURN_AND_NOTIFY_ON_FAILURE(result, log, error_code, ret)  \
  do {                                                              \
    if (!(result)) {                                                \
      DVLOG(1) << log;                                              \
      Destroy();                                                    \
      NotifyError(error_code);                                      \
      return ret;                                                   \
    }                                                               \
  } while (0)

using content::VaapiH264Decoder;

VaapiVideoDecodeAccelerator::InputBuffer::InputBuffer() {
}

VaapiVideoDecodeAccelerator::InputBuffer::~InputBuffer() {
}

void VaapiVideoDecodeAccelerator::NotifyError(Error error) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &VaapiVideoDecodeAccelerator::NotifyError, this, error));
    return;
  }

  DVLOG(1) << "Notifying of error " << error;

  if (client_)
    client_->NotifyError(error);
  client_ = NULL;
}

VaapiVideoDecodeAccelerator::VaapiVideoDecodeAccelerator(Client* client)
    : state_(kUninitialized),
      input_ready_(&lock_),
      output_ready_(&lock_),
      message_loop_(MessageLoop::current()),
      client_(client),
      decoder_thread_("VaapiDecoderThread") {
  DCHECK(client_);
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

  // TODO(posciak): try moving the flag check up to higher layers, possibly
  // out of the GPU process.
  bool res = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVaapi);
  RETURN_AND_NOTIFY_ON_FAILURE(res, "Vaapi HW acceleration disabled",
                               PLATFORM_FAILURE, false);

  res = decoder_.Initialize(
      profile, x_display_, glx_context_,
      base::Bind(&VaapiVideoDecodeAccelerator::OutputPicCallback, this));
  RETURN_AND_NOTIFY_ON_FAILURE(res, "Failed initializing decoder",
                               PLATFORM_FAILURE, false);

  res = decoder_thread_.Start();
  RETURN_AND_NOTIFY_ON_FAILURE(res, "Failed starting decoder thread",
                               PLATFORM_FAILURE, false);

  state_ = kInitialized;

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::NotifyInitializeDone, this));
  return true;
}

void VaapiVideoDecodeAccelerator::NotifyInitializeDone() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  client_->NotifyInitializeDone();
}

// TODO(posciak, fischman): try to move these to constructor parameters,
// but while removing SetEglState from OVDA as well for symmetry.
void VaapiVideoDecodeAccelerator::SetGlxState(Display* x_display,
                                              GLXContext glx_context) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  x_display_ = x_display;
  glx_context_ = glx_context;
}

void VaapiVideoDecodeAccelerator::NotifyInputBufferRead(int input_buffer_id) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  DVLOG(4) << "Notifying end of input buffer " << input_buffer_id;
  if (client_)
    client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
}

void VaapiVideoDecodeAccelerator::SyncAndNotifyPictureReady(int32 input_id,
                                                            int32 output_id) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Sync the contents of the texture.
  RETURN_AND_NOTIFY_ON_FAILURE(decoder_.PutPicToTexture(output_id),
                               "Failed putting picture to texture",
                               PLATFORM_FAILURE, );

  // And notify the client a picture is ready to be displayed.
  media::Picture picture(output_id, input_id);
  DVLOG(4) << "Notifying output picture id " << output_id
           << " for input "<< input_id << " is ready";
  if (client_)
    client_->PictureReady(picture);
}

void VaapiVideoDecodeAccelerator::MapAndQueueNewInputBuffer(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  DVLOG(4) << "Mapping new input buffer id: " << bitstream_buffer.id()
           << " size: " << (int)bitstream_buffer.size();

  scoped_ptr<base::SharedMemory> shm(
      new base::SharedMemory(bitstream_buffer.handle(), true));
  RETURN_AND_NOTIFY_ON_FAILURE(shm->Map(bitstream_buffer.size()),
                              "Failed to map input buffer", UNREADABLE_INPUT,);

  // Set up a new input buffer and queue it for later.
  linked_ptr<InputBuffer> input_buffer(new InputBuffer());
  input_buffer->shm.reset(shm.release());
  input_buffer->id = bitstream_buffer.id();
  input_buffer->size = bitstream_buffer.size();

  base::AutoLock auto_lock(lock_);
  input_buffers_.push(input_buffer);
  input_ready_.Signal();
}

void VaapiVideoDecodeAccelerator::InitialDecodeTask() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  // Try to initialize or resume playback after reset.
  for (;;) {
    if (!GetInputBuffer())
      return;
    DCHECK(curr_input_buffer_.get());

    VaapiH264Decoder::DecResult res = decoder_.DecodeInitial(
        curr_input_buffer_->id);
    switch (res) {
      case VaapiH264Decoder::kReadyToDecode:
        message_loop_->PostTask(FROM_HERE, base::Bind(
            &VaapiVideoDecodeAccelerator::ReadyToDecode, this,
                decoder_.GetRequiredNumOfPictures(),
                gfx::Size(decoder_.pic_width(), decoder_.pic_height())));
        return;

      case VaapiH264Decoder::kNeedMoreStreamData:
        ReturnCurrInputBuffer();
        break;

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

bool VaapiVideoDecodeAccelerator::GetInputBuffer() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  base::AutoLock auto_lock(lock_);

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

void VaapiVideoDecodeAccelerator::ReturnCurrInputBuffer() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  base::AutoLock auto_lock(lock_);
  DCHECK(curr_input_buffer_.get());
  int32 id = curr_input_buffer_->id;
  curr_input_buffer_.reset();
  message_loop_->PostTask(FROM_HERE, base::Bind(
        &VaapiVideoDecodeAccelerator::NotifyInputBufferRead, this, id));
}

bool VaapiVideoDecodeAccelerator::GetOutputBuffers() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  base::AutoLock auto_lock(lock_);

  while (output_buffers_.empty() &&
         (state_ == kDecoding || state_ == kFlushing)) {
    output_ready_.Wait();
  }

  if (state_ != kDecoding && state_ != kFlushing)
    return false;

  while (!output_buffers_.empty()) {
    decoder_.ReusePictureBuffer(output_buffers_.front());
    output_buffers_.pop();
  }

  return true;
}

void VaapiVideoDecodeAccelerator::DecodeTask() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  // Main decode task.
  DVLOG(4) << "Decode task";

  // Try to decode what stream data is (still) in the decoder until we run out
  // of it.
  for (;;) {
    if (!GetInputBuffer())
      // Early exit requested.
      return;
    DCHECK(curr_input_buffer_.get());

    VaapiH264Decoder::DecResult res =
        decoder_.DecodeOneFrame(curr_input_buffer_->id);
    switch (res) {
      case VaapiH264Decoder::kNeedMoreStreamData:
        ReturnCurrInputBuffer();
        break;

      case VaapiH264Decoder::kDecodedFrame:
        // May still have more stream data, continue decoding.
        break;

      case VaapiH264Decoder::kNoOutputAvailable:
        // No more output buffers in the decoder, try getting more or go to
        // sleep waiting for them.
        if (!GetOutputBuffers())
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

void VaapiVideoDecodeAccelerator::ReadyToDecode(int num_pics,
                                                const gfx::Size& size) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  base::AutoLock auto_lock(lock_);
  switch (state_) {
    case kInitialized:
      DVLOG(1) << "Requesting " << num_pics << " pictures of size: "
               << size.width() << "x" << size.height();
      if (client_)
        client_->ProvidePictureBuffers(num_pics, size, GL_TEXTURE_2D);
      state_ = kPicturesRequested;
      break;
    case kIdle:
      state_ = kDecoding;
      decoder_thread_.message_loop()->PostTask(FROM_HERE,
          base::Bind(&VaapiVideoDecodeAccelerator::DecodeTask, this));
      break;
    default:
      NOTREACHED() << "Invalid state";
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
      decoder_thread_.message_loop()->PostTask(FROM_HERE,
          base::Bind(&VaapiVideoDecodeAccelerator::InitialDecodeTask, this));
      break;

    case kPicturesRequested:
      // Waiting for pictures, return.
      break;

    case kDecoding:
      break;

    case kIdle:
      // Need to get decoder into suitable stream location to resume.
      decoder_thread_.message_loop()->PostTask(FROM_HERE,
          base::Bind(&VaapiVideoDecodeAccelerator::InitialDecodeTask, this));
      break;

    default:
      DVLOG(1) << "Decode request from client in invalid state: " << state_;
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
  decoder_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&VaapiVideoDecodeAccelerator::DecodeTask, this));
}

void VaapiVideoDecodeAccelerator::ReusePictureBuffer(int32 picture_buffer_id) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  TRACE_EVENT1("Video Decoder", "VAVDA::ReusePictureBuffer", "Picture id",
               picture_buffer_id);

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

  message_loop_->PostTask(FROM_HERE,
      base::Bind(&VaapiVideoDecodeAccelerator::FinishFlush, this));
}

void VaapiVideoDecodeAccelerator::Flush() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DVLOG(1) << "Got flush request";

  base::AutoLock auto_lock(lock_);
  state_ = kFlushing;
  // Queue a flush task after all existing decoding tasks to clean up.
  decoder_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&VaapiVideoDecodeAccelerator::FlushTask, this));

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

  if (client_)
    client_->NotifyFlushDone();

  DVLOG(1) << "Flush finished";
}

void VaapiVideoDecodeAccelerator::ResetTask() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  // All the decoding tasks from before the reset request from client are done
  // by now, as this task was scheduled after them and client is expected not
  // to call Decode() after Reset() and before NotifyResetDone.
  decoder_.Reset();

  // Return current input buffer, if present.
  if (curr_input_buffer_.get())
    ReturnCurrInputBuffer();

  // And let client know that we are done with reset.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::FinishReset, this));
}

void VaapiVideoDecodeAccelerator::Reset() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DVLOG(1) << "Got reset request";

  // This will make any new decode tasks exit early.
  base::AutoLock auto_lock(lock_);
  state_ = kResetting;

  decoder_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&VaapiVideoDecodeAccelerator::ResetTask, this));

  input_ready_.Signal();
  output_ready_.Signal();
}

void VaapiVideoDecodeAccelerator::FinishReset() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  base::AutoLock auto_lock(lock_);
  if (state_ != kResetting) {
    DCHECK_EQ(state_, kDestroying);
    return;  // We could've gotten destroyed already.
  }

  // Drop all remaining input buffers, if present.
  while (!input_buffers_.empty())
    input_buffers_.pop();

  state_ = kIdle;

  if (client_)
    client_->NotifyResetDone();

  DVLOG(1) << "Reset finished";
}

void VaapiVideoDecodeAccelerator::DestroyTask() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  DVLOG(1) << "DestroyTask";
  base::AutoLock auto_lock(lock_);

  // This is a dummy task to ensure that all tasks on decoder thread have
  // finished, so we can destroy it from ChildThread, as we need to do that
  // on the thread that has the GLX context.

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::FinishDestroy, this));
}

void VaapiVideoDecodeAccelerator::Destroy() {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &VaapiVideoDecodeAccelerator::Destroy, this));
    return;
  }

  if (state_ == kUninitialized || state_ == kDestroying)
    return;

  DVLOG(1) << "Destroying VAVDA";
  base::AutoLock auto_lock(lock_);
  state_ = kDestroying;
  decoder_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&VaapiVideoDecodeAccelerator::DestroyTask, this));
  client_ = NULL;

  input_ready_.Signal();
  output_ready_.Signal();
}

void VaapiVideoDecodeAccelerator::FinishDestroy() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  base::AutoLock auto_lock(lock_);
  // Called from here as we need to be on the thread that has the GLX context
  // as current.
  decoder_.Destroy();
  state_ = kUninitialized;
}

void VaapiVideoDecodeAccelerator::OutputPicCallback(int32 input_id,
                                                    int32 output_id) {
  TRACE_EVENT2("Video Decoder", "VAVDA::OutputPicCallback",
               "Input id", input_id, "Picture id", output_id);
  DVLOG(4) << "Outputting picture, input id: " << input_id
           << " output id: " << output_id;

  // Forward the request to the main thread.
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  message_loop_->PostTask(FROM_HERE,
      base::Bind(&VaapiVideoDecodeAccelerator::SyncAndNotifyPictureReady,
                 this, input_id, output_id));
}

