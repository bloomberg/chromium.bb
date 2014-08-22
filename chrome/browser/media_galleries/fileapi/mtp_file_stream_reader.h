// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_FILE_STREAM_READER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_FILE_STREAM_READER_H_

#include "base/bind.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace base {
class FilePath;
}

namespace storage {
class FileSystemContext;
}

class MTPFileStreamReader
    : public NON_EXPORTED_BASE(storage::FileStreamReader) {
 public:
  MTPFileStreamReader(storage::FileSystemContext* file_system_context,
                      const storage::FileSystemURL& url,
                      int64 initial_offset,
                      const base::Time& expected_modification_time,
                      bool do_media_header_validation);

  virtual ~MTPFileStreamReader();

  // FileStreamReader overrides.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int64 GetLength(
      const net::Int64CompletionCallback& callback) OVERRIDE;

 private:
  void FinishValidateMediaHeader(
      net::IOBuffer* header_buf,
      net::IOBuffer* buf, int buf_len,
      const net::CompletionCallback& callback,
      const base::File::Info& file_info,
      int header_bytes_read);

  void FinishRead(const net::CompletionCallback& callback,
                  const base::File::Info& file_info, int bytes_read);

  void FinishGetLength(const net::Int64CompletionCallback& callback,
                       const base::File::Info& file_info);

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  storage::FileSystemURL url_;
  int64 current_offset_;
  const base::Time expected_modification_time_;

  bool media_header_validated_;

  base::WeakPtrFactory<MTPFileStreamReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MTPFileStreamReader);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_FILE_STREAM_READER_H_
