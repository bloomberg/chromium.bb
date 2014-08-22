// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_WRITE_FILE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_WRITE_FILE_H_

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/operations/operation.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"
#include "net/base/io_buffer.h"
#include "webkit/browser/fileapi/async_file_util.h"

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace chromeos {
namespace file_system_provider {
namespace operations {

// Bridge between fileapi write file and providing extension's write fil
// request.
// Created per request.
class WriteFile : public Operation {
 public:
  WriteFile(extensions::EventRouter* event_router,
            const ProvidedFileSystemInfo& file_system_info,
            int file_handle,
            scoped_refptr<net::IOBuffer> buffer,
            int64 offset,
            int length,
            const storage::AsyncFileUtil::StatusCallback& callback);
  virtual ~WriteFile();

  // Operation overrides.
  virtual bool Execute(int request_id) OVERRIDE;
  virtual void OnSuccess(int request_id,
                         scoped_ptr<RequestValue> result,
                         bool has_more) OVERRIDE;
  virtual void OnError(int request_id,
                       scoped_ptr<RequestValue> result,
                       base::File::Error error) OVERRIDE;

 private:
  int file_handle_;
  scoped_refptr<net::IOBuffer> buffer_;
  int64 offset_;
  int length_;
  const storage::AsyncFileUtil::StatusCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(WriteFile);
};

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_WRITE_FILE_H_
