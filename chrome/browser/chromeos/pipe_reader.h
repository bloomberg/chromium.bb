// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PIPE_READER_H_
#define CHROME_BROWSER_CHROMEOS_PIPE_READER_H_

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"

namespace chromeos {

// Given a named pipe, this class reads data from it and returns it as a string.
// Currently, we are sending login cookies from the Chrome OS login manager to
// Chrome over a named Unix pipe.  We want to replace this with DBus, but
// would like to create a DBus wrapper library to use throughout Chrome OS
// first.  This stopgap lets us get the infrastructure for passing credentials
// to Chrome in place, which will help clean up login jankiness, and also
// refactor our code as we await the DBus stuff.
// TODO(cmasone): get rid of this code and replace with DBus.

class PipeReader {
 public:
  explicit PipeReader(const FilePath& pipe_name)
      : pipe_(NULL),
        pipe_name_(pipe_name.value()) {
  }
  virtual ~PipeReader() {
    if (pipe_)
      fclose(pipe_);
  }

  // Reads data from the pipe up until either a '\n' or |bytes_to_read| bytes.
  virtual std::string Read(const uint32 bytes_to_read);

 protected:
  // For testing.
  PipeReader() : pipe_(NULL) {}

 private:
  FILE *pipe_;
  std::string pipe_name_;

  DISALLOW_COPY_AND_ASSIGN(PipeReader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PIPE_READER_H_
