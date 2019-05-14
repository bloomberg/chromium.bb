// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_frame_validator.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/hash/md5.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"
#include "media/gpu/video_frame_mapper.h"
#include "media/gpu/video_frame_mapper_factory.h"

namespace media {
namespace test {

// static
std::unique_ptr<VideoFrameValidator> VideoFrameValidator::Create(
    const std::vector<std::string>& expected_frame_checksums,
    const VideoPixelFormat validation_format) {
  auto video_frame_validator = base::WrapUnique(
      new VideoFrameValidator(expected_frame_checksums, validation_format));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize VideoFrameValidator.";
    return nullptr;
  }

  return video_frame_validator;
}

VideoFrameValidator::VideoFrameValidator(
    std::vector<std::string> expected_frame_checksums,
    VideoPixelFormat validation_format)
    : expected_frame_checksums_(std::move(expected_frame_checksums)),
      validation_format_(validation_format),
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

  scoped_refptr<const VideoFrame> validated_frame = video_frame;
  // If this is a DMABuf-backed memory frame we need to map it before accessing.
#if defined(OS_CHROMEOS)
  if (validated_frame->storage_type() == VideoFrame::STORAGE_DMABUFS) {
    // Create VideoFrameMapper if not yet created. The decoder's output pixel
    // format is not known yet when creating the VideoFrameValidator. We can
    // only create the VideoFrameMapper upon receiving the first video frame.
    if (!video_frame_mapper_) {
      video_frame_mapper_ =
          VideoFrameMapperFactory::CreateMapper(video_frame->format());
      LOG_ASSERT(video_frame_mapper_) << "Failed to create VideoFrameMapper";
    }
    validated_frame = video_frame_mapper_->Map(std::move(validated_frame));
  }
#endif  // defined(OS_CHROMEOS)

  LOG_ASSERT(validated_frame->IsMappable());
  if (validated_frame->format() != validation_format_) {
    validated_frame =
        ConvertVideoFrame(validated_frame.get(), validation_format_);
  }

  ASSERT_TRUE(validated_frame);
  std::string computed_md5 = ComputeMD5FromVideoFrame(validated_frame.get());

  base::AutoLock auto_lock(frame_validator_lock_);

  if (expected_frame_checksums_.size() > 0) {
    LOG_IF(FATAL, frame_index >= expected_frame_checksums_.size())
        << "Frame number is over than the number of read md5 values in file.";
    const auto& expected_md5 = expected_frame_checksums_[frame_index];
    if (computed_md5 != expected_md5) {
      mismatched_frames_.push_back(
          MismatchedFrameInfo{frame_index, computed_md5, expected_md5});
    }
  }

  num_frames_validating_--;
  frame_validator_cv_.Signal();
}

std::string VideoFrameValidator::ComputeMD5FromVideoFrame(
    const VideoFrame* video_frame) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  base::MD5Context context;
  base::MD5Init(&context);
  VideoFrame::HashFrameForTesting(&context, *video_frame);
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

}  // namespace test
}  // namespace media
