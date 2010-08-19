// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/pipe_reader.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/file_path.h"
#include "base/scoped_ptr.h"

namespace chromeos {

PipeReader::PipeReader(const FilePath& pipe_name)
    : pipe_(NULL),
    pipe_name_(pipe_name.value()) {
}

PipeReader::~PipeReader() {
  if (pipe_)
    fclose(pipe_);
}

std::string PipeReader::Read(const uint32 bytes_to_read) {
  scoped_array<char> buffer(new char[bytes_to_read]);
  if (pipe_ || (pipe_ = fopen(pipe_name_.c_str(), "r"))) {
    const char* to_return = fgets(buffer.get(), bytes_to_read, pipe_);
    if (to_return)
      return to_return;  // auto-coerced to a std::string.
  }
  return std::string();
}

}  // namespace chromeos
