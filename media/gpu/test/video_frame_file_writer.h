// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_FRAME_FILE_WRITER_H_
#define MEDIA_GPU_TEST_VIDEO_FRAME_FILE_WRITER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "media/gpu/test/video_frame_helpers.h"

namespace media {
namespace test {

class VideoFrameMapper;

// The video frame file writer class implements functionality to write video
// frames to file. Currently only the PNG output format is supported.
class VideoFrameFileWriter : public VideoFrameProcessor {
 public:
  ~VideoFrameFileWriter() override;

  // Create an instance of the video frame file writer.
  static std::unique_ptr<VideoFrameFileWriter> Create();

  // Set the folder the video frame files will be written to.
  void SetOutputFolder(const base::FilePath& output_folder);

  // Wait until all currently scheduled frame write operations are done.
  void WaitUntilDone() const;

  // Interface VideoFrameProcessor
  void ProcessVideoFrame(scoped_refptr<const VideoFrame> video_frame,
                         size_t frame_index) override;

 private:
  VideoFrameFileWriter();

  // Initialize the video frame file writer.
  bool Initialize();
  // Destroy the video frame file writer.
  void Destroy();

  // Writes the specified video frame to file on the |file_writer_thread_|.
  void ProcessVideoFrameTask(scoped_refptr<const VideoFrame> video_frame,
                             size_t frame_index);

  // Output folder the frames will be written to.
  base::FilePath output_folder_ GUARDED_BY(frame_writer_lock_);

  // The video frame mapper used to gain access to the raw video frame memory.
  std::unique_ptr<VideoFrameMapper> video_frame_mapper_;

  // The number of frames currently queued for writing.
  size_t num_frames_writing_ GUARDED_BY(frame_writer_lock_);

  // Thread on which video frame writing is done.
  base::Thread frame_writer_thread_;
  mutable base::Lock frame_writer_lock_;
  mutable base::ConditionVariable frame_writer_cv_;

  SEQUENCE_CHECKER(writer_sequence_checker_);
  SEQUENCE_CHECKER(writer_thread_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VideoFrameFileWriter);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_FRAME_FILE_WRITER_H_
