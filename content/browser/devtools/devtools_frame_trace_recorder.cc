// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_frame_trace_recorder.h"

#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event_impl.h"
#include "cc/output/compositor_frame_metadata.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/readback_types.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace content {

namespace {

static base::subtle::Atomic32 frame_data_count = 0;
static int kMaximumFrameDataCount = 150;
static size_t kFrameAreaLimit = 256000;

}  // namespace

class DevToolsFrameTraceRecorderData
    : public base::trace_event::ConvertableToTraceFormat {
 public:
  DevToolsFrameTraceRecorderData(const cc::CompositorFrameMetadata& metadata)
      : metadata_(metadata),
        weak_factory_(this) {
  }

  base::WeakPtr<DevToolsFrameTraceRecorderData> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void FrameCaptured(const SkBitmap& bitmap, ReadbackResponse response) {
    if (response != READBACK_SUCCESS)
      return;
    int current_frame_count = base::subtle::NoBarrier_Load(&frame_data_count);
    if (current_frame_count >= kMaximumFrameDataCount)
      return;
    frame_ = bitmap;
    if (!frame_.drawsNothing())
      base::subtle::NoBarrier_AtomicIncrement(&frame_data_count, 1);
  }

  void CaptureFrame(RenderFrameHostImpl* host) {
    RenderWidgetHostViewBase* view =
        static_cast<RenderWidgetHostViewBase*>(host->GetView());
    if (!view)
      return;
    int current_frame_count = base::subtle::NoBarrier_Load(&frame_data_count);
    if (current_frame_count >= kMaximumFrameDataCount)
      return;
    float scale = metadata_.page_scale_factor;
    float area = metadata_.scrollable_viewport_size.GetArea();
    if (area * scale * scale > kFrameAreaLimit)
      scale = sqrt(kFrameAreaLimit / area);
    gfx::Size snapshot_size(gfx::ToRoundedSize(gfx::ScaleSize(
        metadata_.scrollable_viewport_size, scale)));
    view->CopyFromCompositingSurface(gfx::Rect(), snapshot_size,
        base::Bind(
            &DevToolsFrameTraceRecorderData::FrameCaptured,
            GetWeakPtr()),
        kN32_SkColorType);
  }

  void AppendAsTraceFormat(std::string* out) const override {
    out->append("\"");
    std::vector<unsigned char> data;
    SkAutoLockPixels lock_image(frame_);
    bool encoded = gfx::PNGCodec::Encode(
        reinterpret_cast<unsigned char*>(frame_.getAddr32(0, 0)),
        gfx::PNGCodec::FORMAT_SkBitmap,
        gfx::Size(frame_.width(), frame_.height()),
        frame_.width() * frame_.bytesPerPixel(),
        false, std::vector<gfx::PNGCodec::Comment>(), &data);
    if (encoded) {
      std::string encoded_data;
      base::Base64Encode(
          base::StringPiece(reinterpret_cast<char*>(&data[0]), data.size()),
          &encoded_data);
      out->append(encoded_data);
    }
    out->append("\"");
  }

 private:
  ~DevToolsFrameTraceRecorderData() override {
    if (!frame_.drawsNothing())
      base::subtle::NoBarrier_AtomicIncrement(&frame_data_count, -1);
  }

  cc::CompositorFrameMetadata metadata_;
  SkBitmap frame_;
  base::WeakPtrFactory<DevToolsFrameTraceRecorderData> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsFrameTraceRecorderData);
};

DevToolsFrameTraceRecorder::DevToolsFrameTraceRecorder() { }

DevToolsFrameTraceRecorder::~DevToolsFrameTraceRecorder() { }

void DevToolsFrameTraceRecorder::OnSwapCompositorFrame(
    RenderFrameHostImpl* host,
    const cc::CompositorFrameMetadata& frame_metadata) {
  if (!host)
      return;

  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"), &enabled);
  if (!enabled)
    return;

  if (last_event_data_.get())
    last_event_data_->CaptureFrame(host);

  scoped_refptr<DevToolsFrameTraceRecorderData> data(
      new DevToolsFrameTraceRecorderData(frame_metadata));
  last_event_data_ = data->GetWeakPtr();
  TRACE_EVENT_INSTANT1(
      TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"),
      "CaptureFrame",
      TRACE_EVENT_SCOPE_THREAD,
      "data",
      scoped_refptr<base::trace_event::ConvertableToTraceFormat>(data));
}

}  // namespace content
