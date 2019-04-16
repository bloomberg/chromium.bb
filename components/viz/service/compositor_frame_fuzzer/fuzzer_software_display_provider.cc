// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/compositor_frame_fuzzer/fuzzer_software_display_provider.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display_embedder/software_output_surface.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "ui/gfx/codec/png_codec.h"

namespace viz {

namespace {

// SoftwareOutputDevice that dumps the display's pixmap into a new PNG file per
// paint event. For debugging only: this significantly slows down fuzzing
// iterations and will not handle more than UINT32_MAX files.
class PNGSoftwareOutputDevice : public SoftwareOutputDevice {
 public:
  explicit PNGSoftwareOutputDevice(base::FilePath output_dir)
      : output_dir_(output_dir) {}

  // SoftwareOutputDevice implementation
  void EndPaint() override {
    SkPixmap input_pixmap;
    surface_->peekPixels(&input_pixmap);

    gfx::PNGCodec::ColorFormat color_format;
    switch (input_pixmap.colorType()) {
      case kRGBA_8888_SkColorType:
        color_format = gfx::PNGCodec::FORMAT_RGBA;
        break;
      case kBGRA_8888_SkColorType:
        color_format = gfx::PNGCodec::FORMAT_BGRA;
        break;
      default:
        // failing to find a better default, this one is OK; PNGCodec::Encode
        // will convert this to kN32_SkColorType
        color_format = gfx::PNGCodec::FORMAT_SkBitmap;
        break;
    }

    std::vector<unsigned char> output;
    gfx::PNGCodec::Encode(
        static_cast<const unsigned char*>(input_pixmap.addr()), color_format,
        gfx::Size(input_pixmap.width(), input_pixmap.height()),
        input_pixmap.rowBytes(),
        /*discard_transparency=*/false,
        /*comments=*/{}, &output);

    base::WriteFile(NextOutputFilePath(),
                    reinterpret_cast<char*>(output.data()), output.size());
  }

 private:
  // Return path of next output file. Will not handle overflow of file_id_.
  base::FilePath NextOutputFilePath() {
    // maximum possible length of next_file_id_ (in characters)
    constexpr int kPaddedIntLength = 10;

    std::string file_name =
        base::StringPrintf("%0*u.png", kPaddedIntLength, next_file_id_++);

    return output_dir_.Append(base::FilePath::FromUTF8Unsafe(file_name));
  }

  base::FilePath output_dir_;
  uint32_t next_file_id_ = 0;
};

}  // namespace

FuzzerSoftwareDisplayProvider::FuzzerSoftwareDisplayProvider(
    ServerSharedBitmapManager* server_shared_bitmap_manager,
    base::Optional<base::FilePath> png_dir_path)
    : shared_bitmap_manager_(server_shared_bitmap_manager),
      png_dir_path_(png_dir_path),
      begin_frame_source_(std::make_unique<StubBeginFrameSource>()) {}

FuzzerSoftwareDisplayProvider::~FuzzerSoftwareDisplayProvider() = default;

std::unique_ptr<Display> FuzzerSoftwareDisplayProvider::CreateDisplay(
    const FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    bool gpu_compositing,
    mojom::DisplayClient* display_client,
    BeginFrameSource* begin_frame_source,
    UpdateVSyncParametersCallback update_vsync_callback,
    const RendererSettings& renderer_settings,
    bool send_swap_size_notifications) {
  auto task_runner = base::ThreadTaskRunnerHandle::Get();
  DCHECK(task_runner);

  std::unique_ptr<SoftwareOutputDevice> software_output_device =
      png_dir_path_ ? std::make_unique<PNGSoftwareOutputDevice>(*png_dir_path_)
                    : std::make_unique<SoftwareOutputDevice>();

  auto output_surface = std::make_unique<SoftwareOutputSurface>(
      std::move(software_output_device), std::move(update_vsync_callback));

  auto scheduler = std::make_unique<DisplayScheduler>(
      begin_frame_source_.get(), task_runner.get(),
      output_surface->capabilities().max_frames_pending);

  return std::make_unique<Display>(shared_bitmap_manager_, renderer_settings,
                                   frame_sink_id, std::move(output_surface),
                                   std::move(scheduler), task_runner);
}

uint32_t FuzzerSoftwareDisplayProvider::GetRestartId() const {
  return BeginFrameSource::kNotRestartableId;
}

}  // namespace viz
