// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_FRAME_VALIDATOR_H_
#define MEDIA_GPU_TEST_VIDEO_FRAME_VALIDATOR_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_types.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

class VideoFrame;

namespace test {

class VideoFrameMapper;

// VideoFrameValidator validates the pixel content of each video frame.
// It maps a video frame by using VideoFrameMapper, and converts the mapped
// frame to I420 format to resolve layout differences due to different pixel
// layouts/alignments on different platforms.
// Thereafter, it compares md5 values of the mapped and converted buffer with
// golden md5 values. The golden values are prepared in advance and must be
// identical on all platforms.
// Mapping and verification of a frame is a costly operation and will influence
// performance measurements.
class VideoFrameValidator : public VideoFrameProcessor {
 public:
  struct MismatchedFrameInfo {
    size_t frame_index;
    std::string computed_md5;
    std::string expected_md5;
  };

  // Create an instance of the video frame validator. The calculated checksums
  // will be compared to the values in |expected_frame_checksums|. If no
  // checksums are provided only checksum calculation will be done.
  static std::unique_ptr<VideoFrameValidator> Create(
      const std::vector<std::string>& expected_frame_checksums);

  ~VideoFrameValidator() override;

  // Get the ordered list of calculated frame checksums.
  const std::vector<std::string>& GetFrameChecksums() const;

  // Returns information of frames that don't match golden md5 values.
  // If there is no mismatched frame, returns an empty vector. This function is
  // thread-safe.
  std::vector<MismatchedFrameInfo> GetMismatchedFramesInfo() const;

  // Returns the number of frames that didn't match the golden md5 values. This
  // function is thread-safe.
  size_t GetMismatchedFramesCount() const;

  // Interface VideoFrameProcessor
  void ProcessVideoFrame(scoped_refptr<const VideoFrame> video_frame,
                         size_t frame_index) override;
  // Wait until all currently scheduled frame validations are done. Returns true
  // if no corrupt frames were found. This function might take a long time to
  // complete, depending on the platform.
  bool WaitUntilDone() override;

 private:
  VideoFrameValidator(std::vector<std::string> expected_frame_checksums,
                      std::unique_ptr<VideoFrameMapper> video_frame_mapper);

  // Start the frame validation thread.
  bool Initialize();
  // Stop the frame validation thread.
  void Destroy();

  // Validate the |video_frame|'s content on the |frame_validator_thread_|.
  void ProcessVideoFrameTask(const scoped_refptr<const VideoFrame> video_frame,
                             size_t frame_index);

  // This maps |video_frame|, converts it to I420 format.
  // Returns the resulted I420 frame on success, and otherwise return nullptr.
  // |video_frame| is unchanged in this method.
  // TODO(dstaessens@) Move frame helper functions to video_frame_helpers.h.
  scoped_refptr<VideoFrame> CreateStandardizedFrame(
      scoped_refptr<const VideoFrame> video_frame) const;

  // Returns md5 values of video frame represented by |video_frame|.
  std::string ComputeMD5FromVideoFrame(
      scoped_refptr<VideoFrame> video_frame) const;

  // Helper function to save I420 yuv image.
  bool WriteI420ToFile(size_t frame_index,
                       const VideoFrame* const video_frame) const;

  // The results of invalid frame data.
  std::vector<MismatchedFrameInfo> mismatched_frames_
      GUARDED_BY(frame_validator_lock_);

  // The list of calculated MD5 frame checksums.
  std::vector<std::string> frame_checksums_ GUARDED_BY(frame_validator_lock_);

  // The list of expected MD5 frame checksums.
  const std::vector<std::string> expected_frame_checksums_;

  const std::unique_ptr<VideoFrameMapper> video_frame_mapper_;

  // The number of frames currently queued for validation.
  size_t num_frames_validating_ GUARDED_BY(frame_validator_lock_);

  // Thread on which video frame validation is done.
  base::Thread frame_validator_thread_;
  mutable base::Lock frame_validator_lock_;
  mutable base::ConditionVariable frame_validator_cv_;

  SEQUENCE_CHECKER(validator_sequence_checker_);
  SEQUENCE_CHECKER(validator_thread_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VideoFrameValidator);
};
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_FRAME_VALIDATOR_H_
