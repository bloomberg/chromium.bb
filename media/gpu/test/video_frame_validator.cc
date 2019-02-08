// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_frame_validator.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/md5.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "media/base/video_frame.h"
#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"
#include "media/gpu/test/video_frame_mapper.h"
#include "media/gpu/test/video_frame_mapper_factory.h"

namespace media {
namespace test {

// static
std::unique_ptr<VideoFrameValidator> VideoFrameValidator::Create(
    const std::vector<std::string>& expected_frame_checksums) {
  auto video_frame_mapper = VideoFrameMapperFactory::CreateMapper();
  if (!video_frame_mapper) {
    LOG(ERROR) << "Failed to create VideoFrameMapper.";
    return nullptr;
  }

  auto video_frame_validator = base::WrapUnique(new VideoFrameValidator(
      VideoFrameValidator::CHECK, base::FilePath(), expected_frame_checksums,
      std::move(video_frame_mapper)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize VideoFrameValidator.";
    return nullptr;
  }

  return video_frame_validator;
}

// static
std::unique_ptr<VideoFrameValidator> VideoFrameValidator::Create(
    uint32_t flags,
    const base::FilePath& prefix_output_yuv,
    const std::vector<std::string>& expected_frame_checksums) {
  if ((flags & VideoFrameValidator::OUTPUTYUV) && prefix_output_yuv.empty()) {
    LOG(ERROR) << "Prefix of yuv files isn't specified with dump flags.";
    return nullptr;
  }

  auto video_frame_mapper = VideoFrameMapperFactory::CreateMapper();

  if (!video_frame_mapper) {
    LOG(ERROR) << "Failed to create VideoFrameMapper.";
    return nullptr;
  }

  auto video_frame_validator = base::WrapUnique(new VideoFrameValidator(
      flags, prefix_output_yuv, std::move(expected_frame_checksums),
      std::move(video_frame_mapper)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize VideoFrameValidator.";
    return nullptr;
  }

  return video_frame_validator;
}

VideoFrameValidator::VideoFrameValidator(
    uint32_t flags,
    const base::FilePath& prefix_output_yuv,
    std::vector<std::string> expected_frame_checksums,
    std::unique_ptr<VideoFrameMapper> video_frame_mapper)
    : flags_(flags),
      prefix_output_yuv_(prefix_output_yuv),
      expected_frame_checksums_(std::move(expected_frame_checksums)),
      video_frame_mapper_(std::move(video_frame_mapper)),
      num_frames_validating_(0),
      frame_validator_thread_("FrameValidatorThread"),
      frame_validator_cv_(&frame_validator_lock_) {
  DETACH_FROM_SEQUENCE(validator_sequence_checker_);
  DETACH_FROM_SEQUENCE(validator_thread_sequence_checker_);
}

VideoFrameValidator::~VideoFrameValidator() {
  Destroy();
}

bool VideoFrameValidator::Initialize() {
  if (!frame_validator_thread_.Start()) {
    LOG(ERROR) << "Failed to start frame validator thread";
    return false;
  }
  return true;
}

void VideoFrameValidator::Destroy() {
  frame_validator_thread_.Stop();
  base::AutoLock auto_lock(frame_validator_lock_);
  DCHECK_EQ(0u, num_frames_validating_);
}

const std::vector<std::string>& VideoFrameValidator::GetFrameChecksums() const {
  return frame_checksums_;
}

std::vector<VideoFrameValidator::MismatchedFrameInfo>
VideoFrameValidator::GetMismatchedFramesInfo() const {
  base::AutoLock auto_lock(frame_validator_lock_);
  return mismatched_frames_;
}

size_t VideoFrameValidator::GetMismatchedFramesCount() const {
  base::AutoLock auto_lock(frame_validator_lock_);
  return mismatched_frames_.size();
}

void VideoFrameValidator::ProcessVideoFrame(
    scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_sequence_checker_);

  base::AutoLock auto_lock(frame_validator_lock_);
  num_frames_validating_++;

  // Unretained is safe here, as we should not destroy the validator while there
  // are still frames being validated.
  frame_validator_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameValidator::ProcessVideoFrameTask,
                     base::Unretained(this), video_frame, frame_index));
}

bool VideoFrameValidator::WaitUntilDone() {
  base::AutoLock auto_lock(frame_validator_lock_);
  while (num_frames_validating_ > 0) {
    frame_validator_cv_.Wait();
  }

  if (mismatched_frames_.size() > 0u) {
    LOG(ERROR) << mismatched_frames_.size() << " frames failed to validate.";
    return false;
  }
  return true;
}

void VideoFrameValidator::ProcessVideoFrameTask(
    const scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);

  auto standard_frame = CreateStandardizedFrame(video_frame);
  if (!standard_frame) {
    return;
  }
  std::string computed_md5 = ComputeMD5FromVideoFrame(standard_frame);

  base::AutoLock auto_lock(frame_validator_lock_);
  frame_checksums_.push_back(computed_md5);

  if (flags_ & Flags::CHECK) {
    LOG_IF(FATAL, frame_index >= expected_frame_checksums_.size())
        << "Frame number is over than the number of read md5 values in file.";
    const auto& expected_md5 = expected_frame_checksums_[frame_index];
    if (computed_md5 != expected_md5) {
      mismatched_frames_.push_back(
          MismatchedFrameInfo{frame_index, computed_md5, expected_md5});
    }
  }

  if (flags_ & Flags::OUTPUTYUV) {
    LOG_IF(WARNING, !WriteI420ToFile(frame_index, standard_frame.get()))
        << "Failed to write yuv into file.";
  }

  num_frames_validating_--;
  frame_validator_cv_.Signal();
}

scoped_refptr<VideoFrame> VideoFrameValidator::CreateStandardizedFrame(
    scoped_refptr<const VideoFrame> video_frame) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  auto mapped_frame = video_frame_mapper_->Map(std::move(video_frame));
  if (!mapped_frame) {
    LOG(FATAL) << "Failed to map decoded picture.";
    return nullptr;
  }

  return ConvertVideoFrame(mapped_frame.get(), PIXEL_FORMAT_I420);
}

std::string VideoFrameValidator::ComputeMD5FromVideoFrame(
    scoped_refptr<VideoFrame> video_frame) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  base::MD5Context context;
  base::MD5Init(&context);
  VideoFrame::HashFrameForTesting(&context, *video_frame.get());
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

// TODO(dstaessens@) Move to video_frame_writer.h
bool VideoFrameValidator::WriteI420ToFile(
    size_t frame_index,
    const VideoFrame* const video_frame) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  if (video_frame->format() != PIXEL_FORMAT_I420) {
    LOG(ERROR) << "No I420 format frame.";
    return false;
  }
  if (video_frame->storage_type() !=
      VideoFrame::StorageType::STORAGE_OWNED_MEMORY) {
    LOG(ERROR) << "Video frame doesn't own memory.";
    return false;
  }
  const int width = video_frame->visible_rect().width();
  const int height = video_frame->visible_rect().height();
  base::FilePath::StringType output_yuv_fname;
  base::SStringPrintf(&output_yuv_fname,
                      FILE_PATH_LITERAL("%04zu_%dx%d_I420.yuv"), frame_index,
                      width, height);
  base::File yuv_file(prefix_output_yuv_.AddExtension(output_yuv_fname),
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_APPEND);
  const size_t num_planes = VideoFrame::NumPlanes(video_frame->format());
  for (size_t i = 0; i < num_planes; i++) {
    size_t plane_w = VideoFrame::Columns(i, video_frame->format(), width);
    size_t plane_h = VideoFrame::Rows(i, video_frame->format(), height);
    int data_size = base::checked_cast<int>(plane_w * plane_h);
    const uint8_t* data = video_frame->data(i);
    if (yuv_file.Write(0, reinterpret_cast<const char*>(data), data_size) !=
        data_size) {
      LOG(ERROR) << "Fail to write file in plane #" << i;
      return false;
    }
  }
  return true;
}

}  // namespace test
}  // namespace media
