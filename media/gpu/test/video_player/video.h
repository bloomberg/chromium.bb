// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "media/base/video_codecs.h"

namespace media {
namespace test {

// The video class provides functionality to load video files and manage their
// properties such as video codec, number of frames, frame checksums,...
// TODO(@dstaessens):
// * Use a file stream rather than loading potentially huge files into memory.
class Video {
 public:
  explicit Video(const base::FilePath& file_path);
  ~Video();

  // Load the video file from disk.
  bool Load();
  // Returns true if the video file was loaded.
  bool IsLoaded() const;

  // Get the video file path.
  const base::FilePath& FilePath() const;
  // Get the video data, will be empty if the video hasn't been loaded yet.
  const std::vector<uint8_t>& Data() const;

  // Get the video's codec.
  VideoCodecProfile Profile() const;
  // Get the number of frames in the video.
  uint32_t NumFrames() const;
  // Get the list of frame checksums.
  const std::vector<std::string>& FrameChecksums() const;

  // Set the default path to the test video data.
  static void SetTestDataPath(const base::FilePath& test_data_path);

 private:
  // Return a profile that |codec| represents.
  static VideoCodecProfile ConvertStringtoProfile(const std::string& codec);

  // Load metadata from the JSON file associated with the video file.
  bool LoadMetadata();
  // Return true if video metadata is already loaded.
  bool IsMetadataLoaded() const;

  // Load video frame checksums from the associated checksums file.
  bool LoadFrameChecksums();
  // Return true if the video frame checksums are loaded.
  bool FrameChecksumsLoaded() const;

  // The path where all test video files are stored.
  // TODO(dstaessens@) Avoid using a static data path here.
  static base::FilePath test_data_path_;
  // The video file's path, can be absolute or relative to the above path.
  base::FilePath file_path_;

  // The video's data stream.
  std::vector<uint8_t> data_;

  // Ordered list of video frame checksums.
  std::vector<std::string> frame_checksums_;

  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  uint32_t num_frames_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Video);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_H_
