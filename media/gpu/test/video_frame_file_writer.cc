// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_frame_file_writer.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "media/gpu/test/video_frame_mapper.h"
#include "media/gpu/test/video_frame_mapper_factory.h"
#include "ui/gfx/codec/png_codec.h"

// Default output folder used to store frames.
constexpr const base::FilePath::CharType* kDefaultOutputFolder =
    FILE_PATH_LITERAL("video_frames");

namespace media {
namespace test {

VideoFrameFileWriter::VideoFrameFileWriter()
    : num_frames_writing_(0),
      frame_writer_thread_("FrameWriterThread"),
      frame_writer_cv_(&frame_writer_lock_) {
  DETACH_FROM_SEQUENCE(writer_sequence_checker_);
  DETACH_FROM_SEQUENCE(writer_thread_sequence_checker_);
}

VideoFrameFileWriter::~VideoFrameFileWriter() {
  Destroy();
}

// static
std::unique_ptr<VideoFrameFileWriter> VideoFrameFileWriter::Create() {
  auto frame_file_writer = base::WrapUnique(new VideoFrameFileWriter());
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

  SetOutputFolder(base::FilePath(kDefaultOutputFolder));
  return true;
}

void VideoFrameFileWriter::Destroy() {
  base::AutoLock auto_lock(frame_writer_lock_);
  DCHECK_EQ(0u, num_frames_writing_);

  frame_writer_thread_.Stop();
}

void VideoFrameFileWriter::SetOutputFolder(
    const base::FilePath& output_folder) {
  base::AutoLock auto_lock(frame_writer_lock_);
  DCHECK_EQ(0u, num_frames_writing_);

  output_folder_ = output_folder;

  // If the directory is not absolute, consider it relative to our working dir.
  if (!output_folder_.IsAbsolute()) {
    output_folder_ = base::MakeAbsoluteFilePath(
                         base::FilePath(base::FilePath::kCurrentDirectory))
                         .Append(output_folder_);
  }

  // Create the directory tree if it doesn't exist yet.
  if (!DirectoryExists(output_folder_)) {
    base::CreateDirectory(output_folder_);
  }
}

void VideoFrameFileWriter::WaitUntilDone() const {
  base::AutoLock auto_lock(frame_writer_lock_);
  while (num_frames_writing_ > 0) {
    frame_writer_cv_.Wait();
  }
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

void VideoFrameFileWriter::ProcessVideoFrameTask(
    scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(writer_thread_sequence_checker_);

  auto mapped_frame = video_frame_mapper_->Map(std::move(video_frame));
  if (!mapped_frame) {
    LOG(ERROR) << "Failed to map video frame";
    return;
  }

  auto argb_out_frame = ConvertVideoFrame(mapped_frame.get(),
                                          VideoPixelFormat::PIXEL_FORMAT_ARGB);

  // Convert the ARGB frame to PNG.
  std::vector<uint8_t> png_output;
  const bool png_encode_status = gfx::PNGCodec::Encode(
      argb_out_frame->data(VideoFrame::kARGBPlane), gfx::PNGCodec::FORMAT_BGRA,
      argb_out_frame->visible_rect().size(),
      argb_out_frame->stride(VideoFrame::kARGBPlane),
      true, /* discard_transparency */
      std::vector<gfx::PNGCodec::Comment>(), &png_output);
  LOG_ASSERT(png_encode_status);

  base::FilePath filename;
  {
    base::AutoLock auto_lock(frame_writer_lock_);
    filename = output_folder_.Append("frame_.png")
                   .InsertBeforeExtension(base::IntToString(frame_index));
  }

  // Write the PNG data to file.
  const int size = base::checked_cast<int>(png_output.size());
  const int bytes_written = base::WriteFile(
      filename, reinterpret_cast<char*>(png_output.data()), size);
  LOG_ASSERT(bytes_written == size);

  base::AutoLock auto_lock(frame_writer_lock_);
  num_frames_writing_--;
  frame_writer_cv_.Signal();
}

}  // namespace test
}  // namespace media
