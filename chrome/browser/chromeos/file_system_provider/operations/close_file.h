// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_CLOSE_FILE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_CLOSE_FILE_H_

#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/operations/operation.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"
#include "storage/browser/fileapi/async_file_util.h"

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace chromeos {
namespace file_system_provider {
namespace operations {

// Opens a file for either read or write, with optionally creating the file
// first. Note, that this is part of storage::CreateOrOpen file, but it does
// not download the file locally. Created per request.
class CloseFile : public Operation {
 public:
  CloseFile(extensions::EventRouter* event_router,
            const ProvidedFileSystemInfo& file_system_info,
            int open_request_id,
            const storage::AsyncFileUtil::StatusCallback& callback);
  virtual ~CloseFile();

  // Operation overrides.
  virtual bool Execute(int request_id) OVERRIDE;
  virtual void OnSuccess(int request_id,
                         scoped_ptr<RequestValue> result,
                         bool has_more) OVERRIDE;
  virtual void OnError(int request_id,
                       scoped_ptr<RequestValue> result,
                       base::File::Error error) OVERRIDE;

 private:
  int open_request_id_;
  const storage::AsyncFileUtil::StatusCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(CloseFile);
};

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_CLOSE_FILE_H_
