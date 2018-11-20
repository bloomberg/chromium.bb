// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"

#define VLOGF(level) VLOG(level) << __func__ << "(): "

namespace media {
namespace test {

base::FilePath Video::test_data_path_ = base::FilePath();

Video::Video(const base::FilePath& file_path) : file_path_(file_path) {}

Video::~Video() {}

bool Video::Load() {
  // TODO(dstaessens@) Investigate reusing existing infrastructure such as
  //                   DecoderBuffer.
  DCHECK(!file_path_.empty());
  DCHECK(data_.empty());

  // The specified path can be either an absolute path, a path relative to the
  // current directory, or relative to the test data path.
  if (!file_path_.IsAbsolute()) {
    if (!PathExists(file_path_))
      file_path_ = test_data_path_.Append(file_path_);
    file_path_ = base::MakeAbsoluteFilePath(file_path_);
  }
  VLOGF(2) << "File path: " << file_path_;

  int64_t file_size;
  if (!base::GetFileSize(file_path_, &file_size) || (file_size < 0)) {
    VLOGF(1) << "Failed to read file size: " << file_path_;
    return false;
  }

  std::vector<uint8_t> data(file_size);
  if (base::ReadFile(file_path_, reinterpret_cast<char*>(data.data()),
                     base::checked_cast<int>(file_size)) != file_size) {
    VLOGF(1) << "Failed to read file: " << file_path_;
    return false;
  }

  data_ = std::move(data);
  // TODO(keiitchiw@) Get this from the metadata accompanying each video file.
  profile_ = H264PROFILE_BASELINE;

  return true;
}

bool Video::IsLoaded() const {
  return data_.size() > 0;
}

const std::vector<uint8_t>& Video::GetData() const {
  return data_;
}

VideoCodecProfile Video::GetProfile() const {
  return profile_;
}

// static
void Video::SetTestDataPath(const base::FilePath& test_data_path) {
  test_data_path_ = test_data_path;
}

}  // namespace test
}  // namespace media
