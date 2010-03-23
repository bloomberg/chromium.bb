// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is in interface to OmxCodec from the media playback
// pipeline. It interacts with OmxCodec and the VideoDecoderImpl
// in the media pipeline.
//
// THREADING SEMANTICS
//
// This class is created by VideoDecoderImpl and lives on the thread
// that VideoDecoderImpl lives. This class is given the message loop
// for the above thread. The same message loop is used to host
// OmxCodec which is the interface to the actual OpenMAX hardware.
// OmxCodec gurantees that all the callbacks are executed on the
// hosting message loop. This essentially means that all methods in
// this class are executed on the same thread as VideoDecoderImpl.
// Because of that there's no need for locking anywhere.

#include "media/filters/omx_video_decode_engine.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "media/base/callback.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

OmxVideoDecodeEngine::OmxVideoDecodeEngine()
    : state_(kCreated),
      frame_bytes_(0),
      has_fed_on_eos_(false),
      message_loop_(NULL) {
}

OmxVideoDecodeEngine::~OmxVideoDecodeEngine() {
}

void OmxVideoDecodeEngine::Initialize(AVStream* stream, Task* done_cb) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  AutoTaskRunner done_runner(done_cb);
  omx_codec_ = new media::OmxCodec(message_loop_);

  width_ = stream->codec->width;
  height_ = stream->codec->height;

  // TODO(ajwong): Extract magic formula to something based on output
  // pixel format.
  frame_bytes_ = (width_ * height_ * 3) / 2;
  y_buffer_.reset(new uint8[width_ * height_]);
  u_buffer_.reset(new uint8[width_ * height_ / 4]);
  v_buffer_.reset(new uint8[width_ * height_ / 4]);

  // TODO(ajwong): Find the right way to determine the Omx component name.
  OmxConfigurator::MediaFormat input_format, output_format;
  memset(&input_format, 0, sizeof(input_format));
  memset(&output_format, 0, sizeof(output_format));
  input_format.codec = OmxConfigurator::kCodecH264;
  output_format.codec = OmxConfigurator::kCodecRaw;
  omx_configurator_.reset(
      new OmxDecoderConfigurator(input_format, output_format));
  omx_codec_->Setup(omx_configurator_.get(), this);
  omx_codec_->SetErrorCallback(
      NewCallback(this, &OmxVideoDecodeEngine::OnHardwareError));
  omx_codec_->SetFormatCallback(
      NewCallback(this, &OmxVideoDecodeEngine::OnFormatChange));
  omx_codec_->Start();
  state_ = kNormal;
}

void OmxVideoDecodeEngine::OnFormatChange(
    const OmxConfigurator::MediaFormat& input_format,
    const OmxConfigurator::MediaFormat& output_format) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  // TODO(jiesun): We should not need this for here, because width and height
  // are already known from upper layer of the stack.
}


void OmxVideoDecodeEngine::OnHardwareError() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  state_ = kError;
}

// This method assumes the input buffer contains exactly one frame or is an
// end-of-stream buffer. The logic of this method and in OnReadComplete() is
// based on this assumation.
//
// For every input buffer received here, we submit one read request to the
// decoder. So when a read complete callback is received, a corresponding
// decode request must exist.
void OmxVideoDecodeEngine::DecodeFrame(const Buffer& buffer,
                                       AVFrame* yuv_frame,
                                       bool* got_result,
                                       Task* done_cb) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  if (state_ != kNormal) {
    return;
  }

  if (!has_fed_on_eos_) {
    OmxInputBuffer* input_buffer;
    if (buffer.IsEndOfStream()) {
      input_buffer = new OmxInputBuffer(NULL, 0);
    } else {
      // TODO(ajwong): This is a memcpy() of the compressed frame. Avoid?
      uint8* data = new uint8[buffer.GetDataSize()];
      memcpy(data, buffer.GetData(), buffer.GetDataSize());
      input_buffer = new OmxInputBuffer(data, buffer.GetDataSize());
    }

    omx_codec_->Feed(input_buffer,
                     NewCallback(this, &OmxVideoDecodeEngine::OnFeedDone));

    if (buffer.IsEndOfStream()) {
      has_fed_on_eos_ = true;
    }
  }

  // Enqueue the decode request and the associated buffer.
  decode_request_queue_.push_back(
      DecodeRequest(yuv_frame, got_result, done_cb));

  // Submit a read request to the decoder.
  omx_codec_->Read(NewCallback(this, &OmxVideoDecodeEngine::OnReadComplete));
}

void OmxVideoDecodeEngine::OnFeedDone(OmxInputBuffer* buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  // TODO(ajwong): Add a DoNothingCallback or similar.
}

void OmxVideoDecodeEngine::Flush(Task* done_cb) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  omx_codec_->Flush(TaskToCallbackAdapter::NewCallback(done_cb));
}

VideoFrame::Format OmxVideoDecodeEngine::GetSurfaceFormat() const {
  return VideoFrame::YV12;
}

void OmxVideoDecodeEngine::Stop(Callback0::Type* done_cb) {
  omx_codec_->Stop(done_cb);

  // TODO(hclam): make sure writing to state_ is safe.
  state_ = kStopped;
}

bool OmxVideoDecodeEngine::AllocateEGLImages(
    int width, int height, std::vector<EGLImageKHR>* images) {
  NOTREACHED() << "This method is never used";
  return false;
}

void OmxVideoDecodeEngine::ReleaseEGLImages(
    const std::vector<EGLImageKHR>& images) {
  NOTREACHED() << "This method is never used";
}

void OmxVideoDecodeEngine::UseThisBuffer(int buffer_id,
                                         OMX_BUFFERHEADERTYPE* buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  omx_buffers_.push_back(std::make_pair(buffer_id, buffer));
}

void OmxVideoDecodeEngine::StopUsingThisBuffer(int id) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  omx_buffers_.erase(FindBuffer(id));
}

void OmxVideoDecodeEngine::BufferReady(int buffer_id,
                                       BufferUsedCallback* callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  CHECK((buffer_id == OmxCodec::kEosBuffer) || callback);

  // Obtain the corresponding OMX_BUFFERHEADERTYPE.
  OmxBufferList::iterator iter = FindBuffer(buffer_id);
  if (iter != omx_buffers_.end()) {
    uint8* buffer = iter->second->pBuffer;
    int size = iter->second->nFilledLen;

    if ((size_t)size != frame_bytes_) {
      LOG(ERROR) << "Read completed with weird size: " << size;
    }

    // Merge the buffer into previous buffers.
    MergeBytesFrameQueue(buffer, size);
  }

  // We assume that when we receive a read complete callback from
  // OmxCodec there was a read request made.
  CHECK(!decode_request_queue_.empty());
  const DecodeRequest request = decode_request_queue_.front();
  decode_request_queue_.pop_front();
  *request.got_result = false;

  // Detect if we have received a full decoded frame.
  if (DecodedFrameAvailable()) {
    // |frame| carries the decoded frame.
    scoped_ptr<YuvFrame> frame(yuv_frame_queue_.front());
    yuv_frame_queue_.pop_front();
    *request.got_result = true;

    // TODO(ajwong): This is a memcpy(). Avoid this.
    // TODO(ajwong): This leaks memory. Fix by not using AVFrame.
    const int pixels = width_ * height_;
    request.frame->data[0] = y_buffer_.get();
    request.frame->data[1] = u_buffer_.get();
    request.frame->data[2] = v_buffer_.get();
    request.frame->linesize[0] = width_;
    request.frame->linesize[1] = width_ / 2;
    request.frame->linesize[2] = width_ / 2;

    memcpy(request.frame->data[0], frame->data, pixels);
    memcpy(request.frame->data[1], frame->data + pixels,
           pixels / 4);
    memcpy(request.frame->data[2],
           frame->data + pixels + pixels /4,
           pixels / 4);
  }

  // Notify the read request to this object has been fulfilled.
  request.done_cb->Run();

  // Notify OmxCodec that we have finished using this buffer.
  if (callback) {
    callback->Run(buffer_id);
    delete callback;
  }
}

void OmxVideoDecodeEngine::OnReadComplete(
    int buffer_id, OmxOutputSink::BufferUsedCallback* callback) {
  BufferReady(buffer_id, callback);
}

bool OmxVideoDecodeEngine::IsFrameComplete(const YuvFrame* frame) {
  return frame->size == frame_bytes_;
}

bool OmxVideoDecodeEngine::DecodedFrameAvailable() {
  return (!yuv_frame_queue_.empty() &&
          IsFrameComplete(yuv_frame_queue_.front()));
}

void OmxVideoDecodeEngine::MergeBytesFrameQueue(uint8* buffer, int size) {
  int amount_left = size;

  // TODO(ajwong): Do the swizzle here instead of in DecodeFrame.  This
  // should be able to avoid 1 memcpy.
  while (amount_left > 0) {
    if (yuv_frame_queue_.empty() || IsFrameComplete(yuv_frame_queue_.back())) {
      yuv_frame_queue_.push_back(new YuvFrame(frame_bytes_));
    }
    YuvFrame* frame = yuv_frame_queue_.back();
    int amount_to_copy = std::min((int)(frame_bytes_ - frame->size), size);
    frame->size += amount_to_copy;
    memcpy(frame->data, buffer, amount_to_copy);
    amount_left -= amount_to_copy;
  }
}

OmxVideoDecodeEngine::OmxBufferList::iterator
OmxVideoDecodeEngine::FindBuffer(int buffer_id) {
  for (OmxVideoDecodeEngine::OmxBufferList::iterator i = omx_buffers_.begin();
       i != omx_buffers_.end(); ++i) {
    if (i->first == buffer_id)
      return i;
  }
  return omx_buffers_.end();
}

}  // namespace media
