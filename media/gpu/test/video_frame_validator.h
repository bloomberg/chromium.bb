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
  enum Flags : uint32_t {
    // Checks soundness of video frames.
    CHECK = 1 << 0,
    // Writes out video frames to files.
    OUTPUTYUV = 1 << 1,
    // Writes out md5 values to a file.
    GENMD5 = 1 << 2,
  };

  struct MismatchedFrameInfo {
    size_t frame_index;
    std::string computed_md5;
    std::string expected_md5;
  };

  // Creates an instance of the video frame validator in 'CHECK' mode.
  // |frame_checksums| should contain the ordered list of md5 frame checksums to
  // be used by the validator
  static std::unique_ptr<VideoFrameValidator> Create(
      const std::vector<std::string>& frame_checksums);

  // |flags| decides the behavior of created video frame validator. See the
  // detail in Flags.
  // |prefix_output_yuv| is the prefix name of saved yuv files.
  // VideoFrameValidator saves all I420 video frames.
  // If |prefix_output_yuv_| is not specified, no yuv file will be saved.
  // |md5_file_path| is the path to the file that contains golden md5 values.
  // The file contains one md5 value per line, listed in display order.
  // |linear| represents whether VideoFrame passed on EvaludateVideoFrame() is
  // linear (i.e non-tiled) or not.
  // Returns nullptr on failure.
  static std::unique_ptr<VideoFrameValidator> Create(
      uint32_t flags,
      const base::FilePath& prefix_output_yuv,
      const base::FilePath& md5_file_path,
      bool linear);

  ~VideoFrameValidator() override;

  // Returns information of frames that don't match golden md5 values.
  // If there is no mismatched frame, returns an empty vector. This function is
  // thread-safe.
  std::vector<MismatchedFrameInfo> GetMismatchedFramesInfo() const;

  // Returns the number of frames that didn't match the golden md5 values. This
  // function is thread-safe.
  size_t GetMismatchedFramesCount() const;

  // Wait until all currently scheduled frame validations are done. Returns true
  // if no corrupt frames were found. This function might take a long time to
  // complete, depending on the platform.
  bool WaitUntilValidated() const;

  // Interface VideoFrameProcessor
  void ProcessVideoFrame(scoped_refptr<const VideoFrame> video_frame,
                         size_t frame_index) override;

 private:
  VideoFrameValidator(uint32_t flags,
                      const base::FilePath& prefix_output_yuv,
                      std::vector<std::string> md5_of_frames,
                      base::File md5_file,
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

  // Creates VideoFrame with I420 format from |src_frame|.
  scoped_refptr<VideoFrame> CreateI420Frame(
      const VideoFrame* const src_frame) const;

  // Helper function to save I420 yuv image.
  bool WriteI420ToFile(size_t frame_index,
                       const VideoFrame* const video_frame) const;

  // The results of invalid frame data.
  std::vector<MismatchedFrameInfo> mismatched_frames_
      GUARDED_BY(frame_validator_lock_);

  const uint32_t flags_;

  // Prefix of saved yuv files.
  const base::FilePath prefix_output_yuv_;

  // Golden MD5 values.
  std::vector<std::string> md5_of_frames_;

  // File to write md5 values if flags includes GENMD5.
  base::File md5_file_;

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
