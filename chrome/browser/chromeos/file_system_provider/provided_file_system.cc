// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"

#include "base/files/file.h"
#include "chrome/browser/chromeos/file_system_provider/operations/unmount.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "extensions/browser/event_router.h"

namespace chromeos {
namespace file_system_provider {
namespace {

}  // namespace

ProvidedFileSystem::ProvidedFileSystem(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info)
    : event_router_(event_router), file_system_info_(file_system_info) {
}

ProvidedFileSystem::~ProvidedFileSystem() {}

void ProvidedFileSystem::RequestUnmount(
    const fileapi::AsyncFileUtil::StatusCallback& callback) {
  if (!request_manager_.CreateRequest(
          make_scoped_ptr<RequestManager::HandlerInterface>(
              new operations::Unmount(
                  event_router_, file_system_info_, callback)))) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
  }
}

const ProvidedFileSystemInfo& ProvidedFileSystem::GetFileSystemInfo() const {
  return file_system_info_;
}

RequestManager* ProvidedFileSystem::GetRequestManager() {
  return &request_manager_;
}

}  // namespace file_system_provider
}  // namespace chromeos
