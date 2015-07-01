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

class TraceableDevToolsScreenshot
    : public base::trace_event::ConvertableToTraceFormat {
 public:
  TraceableDevToolsScreenshot(const SkBitmap& bitmap) : frame_(bitmap) {}

  void AppendAsTraceFormat(std::string* out) const override {
    out->append("\"");
    if (!frame_.drawsNothing()) {
      std::vector<unsigned char> data;
      SkAutoLockPixels lock_image(frame_);
      bool encoded = gfx::PNGCodec::Encode(
          reinterpret_cast<unsigned char*>(frame_.getAddr32(0, 0)),
          gfx::PNGCodec::FORMAT_SkBitmap,
          gfx::Size(frame_.width(), frame_.height()),
          frame_.width() * frame_.bytesPerPixel(), false,
          std::vector<gfx::PNGCodec::Comment>(), &data);
      if (encoded) {
        std::string encoded_data;
        base::Base64Encode(
            base::StringPiece(reinterpret_cast<char*>(&data[0]), data.size()),
            &encoded_data);
        out->append(encoded_data);
      }
    }
    out->append("\"");
  }

 private:
  ~TraceableDevToolsScreenshot() override {
    base::subtle::NoBarrier_AtomicIncrement(&frame_data_count, -1);
  }

  SkBitmap frame_;
};

}  // namespace

class DevToolsFrameTraceRecorderData
    : public base::RefCounted<DevToolsFrameTraceRecorderData> {
 public:
  DevToolsFrameTraceRecorderData(const cc::CompositorFrameMetadata& metadata)
      : metadata_(metadata), timestamp_(base::TraceTicks::Now()) {}

  void FrameCaptured(const SkBitmap& bitmap, ReadbackResponse response) {
    if (response != READBACK_SUCCESS)
      return;
    int current_frame_count = base::subtle::NoBarrier_Load(&frame_data_count);
    if (current_frame_count >= kMaximumFrameDataCount)
      return;
    if (bitmap.drawsNothing())
      return;
    base::subtle::NoBarrier_AtomicIncrement(&frame_data_count, 1);
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID_AND_TIMESTAMP(
        TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"), "Screenshot", 1,
        timestamp_.ToInternalValue(),
        scoped_refptr<base::trace_event::ConvertableToTraceFormat>(
            new TraceableDevToolsScreenshot(bitmap)));
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
    view->CopyFromCompositingSurface(
        gfx::Rect(), snapshot_size,
        base::Bind(&DevToolsFrameTraceRecorderData::FrameCaptured, this),
        kN32_SkColorType);
  }

 private:
  friend class base::RefCounted<DevToolsFrameTraceRecorderData>;
  ~DevToolsFrameTraceRecorderData() {}

  cc::CompositorFrameMetadata metadata_;
  base::TraceTicks timestamp_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsFrameTraceRecorderData);
};

DevToolsFrameTraceRecorder::DevToolsFrameTraceRecorder() { }

DevToolsFrameTraceRecorder::~DevToolsFrameTraceRecorder() { }

void DevToolsFrameTraceRecorder::OnSwapCompositorFrame(
    RenderFrameHostImpl* host,
    const cc::CompositorFrameMetadata& frame_metadata,
    bool do_capture) {
  if (!host)
    return;

  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"), &enabled);
  if (!enabled || !do_capture) {
    pending_frame_data_ = nullptr;
    return;
  }
  if (pending_frame_data_.get())
    pending_frame_data_->CaptureFrame(host);
  pending_frame_data_ = new DevToolsFrameTraceRecorderData(frame_metadata);
}

}  // namespace content
