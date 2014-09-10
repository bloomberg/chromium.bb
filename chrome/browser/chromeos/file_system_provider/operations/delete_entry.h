// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_DELETE_ENTRY_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_DELETE_ENTRY_H_

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

// Deletes an entry. If |recursive| is passed and the entry is a directory, then
// all contents of it (recursively) will be deleted too. Created per request.
class DeleteEntry : public Operation {
 public:
  DeleteEntry(extensions::EventRouter* event_router,
              const ProvidedFileSystemInfo& file_system_info,
              const base::FilePath& entry_path,
              bool recursive,
              const storage::AsyncFileUtil::StatusCallback& callback);
  virtual ~DeleteEntry();

  // Operation overrides.
  virtual bool Execute(int request_id) OVERRIDE;
  virtual void OnSuccess(int request_id,
                         scoped_ptr<RequestValue> result,
                         bool has_more) OVERRIDE;
  virtual void OnError(int request_id,
                       scoped_ptr<RequestValue> result,
                       base::File::Error error) OVERRIDE;

 private:
  base::FilePath entry_path_;
  ProvidedFileSystemInterface::OpenFileMode mode_;
  bool recursive_;
  const storage::AsyncFileUtil::StatusCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteEntry);
};

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_DELETE_ENTRY_H_
