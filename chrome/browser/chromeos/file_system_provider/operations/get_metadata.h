// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_GET_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_GET_METADATA_H_

#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/operations/operation.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace chromeos {
namespace file_system_provider {
namespace operations {

// Bridge between fileapi get metadata operation and providing extension's get
// metadata request. Created per request.
class GetMetadata : public Operation {
 public:
  GetMetadata(extensions::EventRouter* event_router,
              const ProvidedFileSystemInfo& file_system_info,
              const base::FilePath& entry_path,
              ProvidedFileSystemInterface::MetadataFieldMask fields,
              const ProvidedFileSystemInterface::GetMetadataCallback& callback);
  virtual ~GetMetadata();

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
  ProvidedFileSystemInterface::MetadataFieldMask fields_;
  const ProvidedFileSystemInterface::GetMetadataCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetMetadata);
};

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_GET_METADATA_H_
