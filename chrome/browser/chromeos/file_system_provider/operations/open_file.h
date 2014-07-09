// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_OPEN_FILE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_OPEN_FILE_H_

#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/operations/operation.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"
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

// Opens a file for either read or write. The file must exist, otherwise the
// operation will fail. Created per request.
class OpenFile : public Operation {
 public:
  OpenFile(extensions::EventRouter* event_router,
           const ProvidedFileSystemInfo& file_system_info,
           const base::FilePath& file_path,
           ProvidedFileSystemInterface::OpenFileMode mode,
           const ProvidedFileSystemInterface::OpenFileCallback& callback);
  virtual ~OpenFile();

  // Operation overrides.
  virtual bool Execute(int request_id) OVERRIDE;
  virtual void OnSuccess(int request_id,
                         scoped_ptr<RequestValue> result,
                         bool has_more) OVERRIDE;
  virtual void OnError(int request_id,
                       scoped_ptr<RequestValue> result,
                       base::File::Error error) OVERRIDE;

 private:
  base::FilePath file_path_;
  ProvidedFileSystemInterface::OpenFileMode mode_;
  const ProvidedFileSystemInterface::OpenFileCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(OpenFile);
};

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_OPEN_FILE_H_
