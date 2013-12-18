// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace image_writer {

using content::BrowserThread;

WriteFromFileOperation::WriteFromFileOperation(
    base::WeakPtr<OperationManager> manager,
    const ExtensionId& extension_id,
    const base::FilePath& path,
    const std::string& storage_unit_id)
  : Operation(manager, extension_id, storage_unit_id) {
    image_path_ = path;
}

WriteFromFileOperation::~WriteFromFileOperation() {
}

void WriteFromFileOperation::Start() {
  DVLOG(1) << "Starting file-to-usb write of " << image_path_.value()
           << " to " << storage_unit_id_;

  if (!base::PathExists(image_path_) || base::DirectoryExists(image_path_)) {
    Error(error::kImageInvalid);
    return;
  }

  WriteStart();
}

}  // namespace image_writer
}  // namespace extensions
