// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_STREAM_READER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_STREAM_READER_H_

#include "base/basictypes.h"
#include "webkit/blob/file_stream_reader.h"

namespace drive {

class DriveFileStreamReader : public webkit_blob::FileStreamReader {
 public:
  DriveFileStreamReader();
  virtual ~DriveFileStreamReader();

  // webkit_blob::FileStreamReader overrides.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int GetLength(const net::Int64CompletionCallback& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFileStreamReader);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_STREAM_READER_H_
