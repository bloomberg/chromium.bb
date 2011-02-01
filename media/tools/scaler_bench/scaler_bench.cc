// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tool can be used to measure performace of video frame scaling
// code. It times performance of the scaler with and without filtering.
// It also measures performance of the Skia scaler for comparison.

#include <iostream>
#include <vector>

#include "base/command_line.h"
#include "base/scoped_vector.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"
#include "skia/ext/platform_canvas.h"

using base::TimeDelta;
using base::TimeTicks;
using media::VideoFrame;

namespace {

int source_width = 1280;
int source_height = 720;
int dest_width = 1366;
int dest_height = 768;
int num_frames = 500;
int num_buffers = 50;

double BenchmarkSkia() {
  std::vector<scoped_refptr<VideoFrame> > source_frames;
  ScopedVector<SkBitmap> dest_frames;
  for (int i = 0; i < num_buffers; i++) {
    scoped_refptr<VideoFrame> source_frame;
    VideoFrame::CreateBlackFrame(source_width, source_height, &source_frame);
    source_frames.push_back(source_frame);

    SkBitmap* bitmap = new SkBitmap();
    bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                      dest_width, dest_height);
    bitmap->allocPixels();
    dest_frames.push_back(bitmap);
  }

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, source_width, source_height);
  bitmap.allocPixels();

  TimeTicks start = TimeTicks::HighResNow();
  for (int i = 0; i < num_frames; i++) {
    scoped_refptr<VideoFrame> source_frame = source_frames[i % num_buffers];
    SkBitmap* dest_bitmap = dest_frames[i % num_buffers];

    bitmap.lockPixels();
    media::ConvertYUVToRGB32(source_frame->data(VideoFrame::kYPlane),
                             source_frame->data(VideoFrame::kUPlane),
                             source_frame->data(VideoFrame::kVPlane),
                             static_cast<uint8*>(bitmap.getPixels()),
                             source_width,
                             source_height,
                             source_frame->stride(VideoFrame::kYPlane),
                             source_frame->stride(VideoFrame::kUPlane),
                             bitmap.rowBytes(),
                             media::YV12);
    bitmap.unlockPixels();

    SkCanvas canvas(*dest_bitmap);
    SkRect rect;
    rect.set(0, 0, SkIntToScalar(dest_width),
             SkIntToScalar(dest_height));
    canvas.clipRect(rect);
    SkMatrix matrix;
    matrix.reset();
    matrix.preScale(SkIntToScalar(dest_width) /
                    SkIntToScalar(source_width),
                    SkIntToScalar(dest_height) /
                    SkIntToScalar(source_height));
    SkPaint paint;
    paint.setFlags(SkPaint::kFilterBitmap_Flag);
    canvas.drawBitmapMatrix(bitmap, matrix, &paint);
  }
  TimeTicks end = TimeTicks::HighResNow();
  return static_cast<double>((end - start).InMilliseconds()) / num_frames;
}

double BenchmarkFilter(media::ScaleFilter filter) {
  std::vector<scoped_refptr<VideoFrame> > source_frames;
  std::vector<scoped_refptr<VideoFrame> > dest_frames;

  for (int i = 0; i < num_buffers; i++) {
    scoped_refptr<VideoFrame> source_frame;
    VideoFrame::CreateBlackFrame(source_width, source_height, &source_frame);
    source_frames.push_back(source_frame);

    scoped_refptr<VideoFrame> dest_frame;
    VideoFrame::CreateFrame(VideoFrame::RGB32,
                            dest_width,
                            dest_height,
                            TimeDelta::FromSeconds(0),
                            TimeDelta::FromSeconds(0),
                            &dest_frame);
    dest_frames.push_back(dest_frame);
  }

  TimeTicks start = TimeTicks::HighResNow();
  for (int i = 0; i < num_frames; i++) {
    scoped_refptr<VideoFrame> source_frame = source_frames[i % num_buffers];
    scoped_refptr<VideoFrame> dest_frame = dest_frames[i % num_buffers];

    media::ScaleYUVToRGB32(source_frame->data(VideoFrame::kYPlane),
                           source_frame->data(VideoFrame::kUPlane),
                           source_frame->data(VideoFrame::kVPlane),
                           dest_frame->data(0),
                           source_width,
                           source_height,
                           dest_width,
                           dest_height,
                           source_frame->stride(VideoFrame::kYPlane),
                           source_frame->stride(VideoFrame::kUPlane),
                           dest_frame->stride(0),
                           media::YV12,
                           media::ROTATE_0,
                           filter);
  }
  TimeTicks end = TimeTicks::HighResNow();
  return static_cast<double>((end - start).InMilliseconds()) / num_frames;
}

}  // namespace

int main(int argc, const char** argv) {
  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  if (!cmd_line->args().empty()) {
    std::cerr << "Usage: " << argv[0] << " [OPTIONS]\n"
              << "  --frames=N                      "
              << "Number of frames\n"
              << "  --buffers=N                     "
              << "Number of buffers\n"
              << "  --src-w=N                       "
              << "Width of the source image\n"
              << "  --src-h=N                       "
              << "Height of the source image\n"
              << "  --dest-w=N                      "
              << "Width of the destination image\n"
              << "  --dest-h=N                      "
              << "Height of the destination image\n"
              << std::endl;
    return 1;
  }

  std::string source_width_param(cmd_line->GetSwitchValueASCII("src-w"));
  if (!source_width_param.empty() &&
      !base::StringToInt(source_width_param, &source_width)) {
    source_width = 0;
  }

  std::string source_height_param(cmd_line->GetSwitchValueASCII("src-h"));
  if (!source_height_param.empty() &&
      !base::StringToInt(source_height_param, &source_height)) {
    source_height = 0;
  }

  std::string dest_width_param(cmd_line->GetSwitchValueASCII("dest-w"));
  if (!dest_width_param.empty() &&
      !base::StringToInt(dest_width_param, &dest_width)) {
    dest_width = 0;
  }

  std::string dest_height_param(cmd_line->GetSwitchValueASCII("dest-h"));
  if (!dest_height_param.empty() &&
      !base::StringToInt(dest_height_param, &dest_height)) {
    dest_height = 0;
  }

  std::string frames_param(cmd_line->GetSwitchValueASCII("frames"));
  if (!frames_param.empty() &&
      !base::StringToInt(frames_param, &num_frames)) {
    num_frames = 0;
  }

  std::string buffers_param(cmd_line->GetSwitchValueASCII("buffers"));
  if (!buffers_param.empty() &&
      !base::StringToInt(buffers_param, &num_buffers)) {
    num_buffers = 0;
  }

  std::cout << "Source image size: " << source_width
            << "x" << source_height << std::endl;
  std::cout << "Destination image size: " << dest_width
            << "x" << dest_height << std::endl;
  std::cout << "Number of frames: " << num_frames << std::endl;
  std::cout << "Number of buffers: " << num_buffers << std::endl;

  std::cout << "Skia: " << BenchmarkSkia()
            << "ms/frame" << std::endl;
  std::cout << "No filtering: " << BenchmarkFilter(media::FILTER_NONE)
            << "ms/frame" << std::endl;
  std::cout << "Bilinear Vertical: "
            << BenchmarkFilter(media::FILTER_BILINEAR_V)
            << "ms/frame" << std::endl;
  std::cout << "Bilinear Horizontal: "
            << BenchmarkFilter(media::FILTER_BILINEAR_H)
            << "ms/frame" << std::endl;
  std::cout << "Bilinear: " << BenchmarkFilter(media::FILTER_BILINEAR)
            << "ms/frame" << std::endl;

  return 0;
}
