// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FRAME_RECORDER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FRAME_RECORDER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/devtools/protocol/devtools_protocol_handler.h"
#include "content/public/browser/readback_types.h"

class SkBitmap;

namespace content {

class RenderViewHostImpl;

namespace devtools {
namespace page {

using EncodedFrame = std::pair<std::string, double>;

class FrameRecorder {
 public:
  using Response = DevToolsProtocolClient::Response;
  using StopRecordingFramesCallback =
      base::Callback<void(scoped_refptr<StopRecordingFramesResponse>)>;

  explicit FrameRecorder();
  virtual ~FrameRecorder();

  Response StartRecordingFrames(int max_frame_count);
  Response StopRecordingFrames(StopRecordingFramesCallback callback);
  Response CancelRecordingFrames();

  void SetRenderViewHost(RenderViewHostImpl* host);
  void OnSwapCompositorFrame();

 private:
  enum State {
    Ready,
    Recording,
    Encoding
  };

  void FrameCaptured(const SkBitmap& bitmap, ReadbackResponse response);
  void FrameEncoded(const scoped_ptr<EncodedFrame>& encoded_frame);
  void MaybeSendResponse();

  RenderViewHostImpl* host_;
  State state_;
  StopRecordingFramesCallback callback_;
  size_t inflight_requests_count_;
  size_t max_frame_count_;
  size_t captured_frames_count_;
  base::Time last_captured_frame_timestamp_;
  std::vector<scoped_refptr<devtools::page::RecordedFrame>> frames_;

  base::CancelableCallback<void(const scoped_ptr<EncodedFrame>&)>
      frame_encoded_callback_;
  base::WeakPtrFactory<FrameRecorder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameRecorder);
};

}  // namespace page
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FRAME_RECORDER_H_
