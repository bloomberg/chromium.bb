// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_frame_file_writer.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "media/gpu/test/video_frame_mapper.h"
#include "media/gpu/test/video_frame_mapper_factory.h"
#include "ui/gfx/codec/png_codec.h"

namespace media {
namespace test {

VideoFrameFileWriter::VideoFrameFileWriter(const base::FilePath& output_folder,
                                           OutputFormat output_format)
    : output_folder_(output_folder),
      output_format_(output_format),
      num_frames_writing_(0),
      frame_writer_thread_("FrameWriterThread"),
      frame_writer_cv_(&frame_writer_lock_) {
  DETACH_FROM_SEQUENCE(writer_sequence_checker_);
  DETACH_FROM_SEQUENCE(writer_thread_sequence_checker_);
}

VideoFrameFileWriter::~VideoFrameFileWriter() {
  base::AutoLock auto_lock(frame_writer_lock_);
  DCHECK_EQ(0u, num_frames_writing_);

  frame_writer_thread_.Stop();
}

// static
std::unique_ptr<VideoFrameFileWriter> VideoFrameFileWriter::Create(
    const base::FilePath& output_folder,
    OutputFormat output_format) {
  // If the directory is not absolute, consider it relative to our working dir.
  base::FilePath resolved_output_folder(output_folder);
  if (!resolved_output_folder.IsAbsolute()) {
    resolved_output_folder =
        base::MakeAbsoluteFilePath(
            base::FilePath(base::FilePath::kCurrentDirectory))
            .Append(resolved_output_folder);
  }

  // Create the directory tree if it doesn't exist yet.
  if (!DirectoryExists(resolved_output_folder)) {
    base::CreateDirectory(resolved_output_folder);
  }

  auto frame_file_writer = base::WrapUnique(
      new VideoFrameFileWriter(resolved_output_folder, output_format));
  if (!frame_file_writer->Initialize()) {
    LOG(ERROR) << "Failed to initialize VideoFrameFileWriter";
    return nullptr;
  }

  return frame_file_writer;
}

bool VideoFrameFileWriter::Initialize() {
  video_frame_mapper_ = VideoFrameMapperFactory::CreateMapper();
  if (!video_frame_mapper_) {
    LOG(ERROR) << "Failed to create VideoFrameMapper";
    return false;
  }

  if (!frame_writer_thread_.Start()) {
    LOG(ERROR) << "Failed to start file writer thread";
    return false;
  }

  return true;
}

void VideoFrameFileWriter::ProcessVideoFrame(
    scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  base::AutoLock auto_lock(frame_writer_lock_);
  num_frames_writing_++;

  // Unretained is safe here, as we should not destroy the writer while there
  // are still frames being written.
  frame_writer_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameFileWriter::ProcessVideoFrameTask,
                     base::Unretained(this), video_frame, frame_index));
}

bool VideoFrameFileWriter::WaitUntilDone() {
  base::AutoLock auto_lock(frame_writer_lock_);
  while (num_frames_writing_ > 0) {
    frame_writer_cv_.Wait();
  }
  return true;
}

void VideoFrameFileWriter::ProcessVideoFrameTask(
    scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(writer_thread_sequence_checker_);

  base::FilePath::StringType filename;
  const gfx::Size& visible_size = video_frame->visible_rect().size();
  base::SStringPrintf(&filename, FILE_PATH_LITERAL("frame_%04zu_%dx%d"),
                      frame_index, visible_size.width(), visible_size.height());

  switch (output_format_) {
    case OutputFormat::kPNG:
      WriteVideoFramePNG(video_frame, base::FilePath(filename));
      break;
    case OutputFormat::kYUV:
      WriteVideoFrameYUV(video_frame, base::FilePath(filename));
      break;
  }

  base::AutoLock auto_lock(frame_writer_lock_);
  num_frames_writing_--;
  frame_writer_cv_.Signal();
}

void VideoFrameFileWriter::WriteVideoFramePNG(
    scoped_refptr<const VideoFrame> video_frame,
    const base::FilePath& filename) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(writer_thread_sequence_checker_);

  auto mapped_frame = video_frame_mapper_->Map(std::move(video_frame));
  if (!mapped_frame) {
    LOG(ERROR) << "Failed to map video frame";
    return;
  }

  scoped_refptr<VideoFrame> argb_out_frame = mapped_frame;
  if (argb_out_frame->format() != PIXEL_FORMAT_ARGB) {
    argb_out_frame = ConvertVideoFrame(argb_out_frame.get(),
                                       VideoPixelFormat::PIXEL_FORMAT_ARGB);
  }

  // Convert the ARGB frame to PNG.
  std::vector<uint8_t> png_output;
  const bool png_encode_status = gfx::PNGCodec::Encode(
      argb_out_frame->data(VideoFrame::kARGBPlane), gfx::PNGCodec::FORMAT_BGRA,
      argb_out_frame->visible_rect().size(),
      argb_out_frame->stride(VideoFrame::kARGBPlane),
      true, /* discard_transparency */
      std::vector<gfx::PNGCodec::Comment>(), &png_output);
  LOG_ASSERT(png_encode_status);

  // Write the PNG data to file.
  base::FilePath file_path(
      output_folder_.Append(filename).AddExtension(FILE_PATH_LITERAL(".png")));
  const int size = base::checked_cast<int>(png_output.size());
  const int bytes_written = base::WriteFile(
      file_path, reinterpret_cast<char*>(png_output.data()), size);
  LOG_ASSERT(bytes_written == size);
}

void VideoFrameFileWriter::WriteVideoFrameYUV(
    scoped_refptr<const VideoFrame> video_frame,
    const base::FilePath& filename) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(writer_thread_sequence_checker_);

  auto mapped_frame = video_frame_mapper_->Map(std::move(video_frame));
  if (!mapped_frame) {
    LOG(ERROR) << "Failed to map video frame";
    return;
  }

  scoped_refptr<VideoFrame> I420_out_frame = mapped_frame;
  if (I420_out_frame->format() != PIXEL_FORMAT_I420) {
    I420_out_frame = ConvertVideoFrame(I420_out_frame.get(),
                                       VideoPixelFormat::PIXEL_FORMAT_I420);
  }

  // Write the YUV data to file.
  base::FilePath file_path(
      output_folder_.Append(filename)
          .AddExtension(FILE_PATH_LITERAL(".yuv"))
          .InsertBeforeExtension(FILE_PATH_LITERAL("_I420")));
  base::File yuv_file(file_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  // TODO(dstaessens@) Take stride into account when visible and coded sizes are
  // different.
  LOG_ASSERT(I420_out_frame->visible_rect().size() ==
             I420_out_frame->coded_size());
  const int width = I420_out_frame->visible_rect().width();
  const int height = I420_out_frame->visible_rect().height();
  const size_t num_planes = VideoFrame::NumPlanes(I420_out_frame->format());
  for (size_t i = 0; i < num_planes; i++) {
    size_t plane_w = VideoFrame::Columns(i, I420_out_frame->format(), width);
    size_t plane_h = VideoFrame::Rows(i, I420_out_frame->format(), height);
    int data_size = base::checked_cast<int>(plane_w * plane_h);
    const uint8_t* data = I420_out_frame->data(i);
    if (yuv_file.WriteAtCurrentPos(reinterpret_cast<const char*>(data),
                                   data_size) != data_size) {
      LOG(ERROR) << "Failed to write plane #" << i << " to file.";
    }
  }
}

}  // namespace test
}  // namespace media
