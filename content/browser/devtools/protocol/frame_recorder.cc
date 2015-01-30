// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/frame_recorder.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

namespace content {
namespace devtools {
namespace page {

namespace {

static int kMaxRecordFrameCount = 180;

scoped_ptr<EncodedFrame> EncodeFrame(
    const SkBitmap& bitmap, double timestamp) {
  std::vector<unsigned char> data;
  SkAutoLockPixels lock_image(bitmap);
  bool encoded = gfx::PNGCodec::Encode(
      reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
      gfx::PNGCodec::FORMAT_SkBitmap,
      gfx::Size(bitmap.width(), bitmap.height()),
      bitmap.width() * bitmap.bytesPerPixel(),
      false, std::vector<gfx::PNGCodec::Comment>(), &data);

  scoped_ptr<EncodedFrame> result(new EncodedFrame(std::string(), timestamp));

  if (!encoded)
    return result.Pass();

  std::string base_64_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<char*>(&data[0]), data.size()),
      &result->first);

  return result.Pass();
}
} // namespace

typedef DevToolsProtocolClient::Response Response;

FrameRecorder::FrameRecorder()
    : host_(nullptr),
      state_(Ready),
      inflight_requests_count_(0),
      max_frame_count_(0),
      captured_frames_count_(0),
      last_captured_frame_timestamp_(base::Time()),
      weak_factory_(this) {
}

FrameRecorder::~FrameRecorder() {
}

void FrameRecorder::SetRenderViewHost(RenderViewHostImpl* host) {
  host_ = host;
}

Response FrameRecorder::StartRecordingFrames(int max_frame_count) {
  if (max_frame_count <= 0 || max_frame_count > kMaxRecordFrameCount)
    return Response::InvalidParams("maxFrameCount");
  if (state_ != Ready)
    return Response::InternalError("Already recording");
  state_ = Recording;
  max_frame_count_ = max_frame_count;
  captured_frames_count_ = 0;
  frame_encoded_callback_.Reset(base::Bind(
      &FrameRecorder::FrameEncoded, weak_factory_.GetWeakPtr()));
  last_captured_frame_timestamp_ = base::Time();
  std::vector<scoped_refptr<devtools::page::RecordedFrame>> frames;
  frames.reserve(max_frame_count);
  frames_.swap(frames);

  return Response::OK();
}

Response FrameRecorder::StopRecordingFrames(
    StopRecordingFramesCallback callback) {
  if (state_ != Recording)
    return Response::InternalError("Not recording");
  state_ = Encoding;
  callback_ = callback;
  MaybeSendResponse();
  return Response::OK();
}

Response FrameRecorder::CancelRecordingFrames() {
  frame_encoded_callback_.Cancel();
  std::vector<scoped_refptr<devtools::page::RecordedFrame>> no_frames;
  frames_.swap(no_frames);
  if (state_ == Encoding)
    callback_.Run(StopRecordingFramesResponse::Create()->set_frames(frames_));
  state_ = Ready;
  return Response::OK();
}

void FrameRecorder::OnSwapCompositorFrame() {
  if (!host_ || state_ != Recording)
    return;
  if (captured_frames_count_ >= max_frame_count_)
    return;
  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(host_->GetView());
  if (!view)
    return;

  inflight_requests_count_++;
  view->CopyFromCompositingSurface(
      gfx::Rect(),
      gfx::Size(),
      base::Bind(&FrameRecorder::FrameCaptured, weak_factory_.GetWeakPtr()),
      kN32_SkColorType);
}

void FrameRecorder::FrameCaptured(
    const SkBitmap& bitmap, ReadbackResponse response) {
  inflight_requests_count_--;
  base::Time timestamp = last_captured_frame_timestamp_;
  last_captured_frame_timestamp_ = base::Time::Now();
  if (timestamp.is_null() || response != READBACK_SUCCESS) {
    MaybeSendResponse();
    return;
  }

  captured_frames_count_++;
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true).get(),
      FROM_HERE,
      base::Bind(&EncodeFrame, bitmap, timestamp.ToDoubleT()),
      frame_encoded_callback_.callback());
}

void FrameRecorder::FrameEncoded(
    const scoped_ptr<EncodedFrame>& encoded_frame) {
  frames_.push_back(RecordedFrame::Create()
      ->set_data(encoded_frame->first)
      ->set_timestamp(encoded_frame->second));
  MaybeSendResponse();
}

void FrameRecorder::MaybeSendResponse() {
  if (state_ != Encoding)
    return;
  if (inflight_requests_count_ || frames_.size() != captured_frames_count_)
    return;
  callback_.Run(StopRecordingFramesResponse::Create()->set_frames(frames_));
  std::vector<scoped_refptr<devtools::page::RecordedFrame>> frames;
  frames_.swap(frames);
  state_ = Ready;
}

}  // namespace page
}  // namespace devtools
}  // namespace content
