// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"

#include "base/files/file.h"
#include "base/message_loop/message_loop_proxy.h"
#include "extensions/browser/event_router.h"

namespace chromeos {
namespace file_system_provider {

FakeProvidedFileSystem::FakeProvidedFileSystem(
    const ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info) {
}

FakeProvidedFileSystem::~FakeProvidedFileSystem() {}

void FakeProvidedFileSystem::RequestUnmount(
    const fileapi::AsyncFileUtil::StatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_OK));
}

void FakeProvidedFileSystem::GetMetadata(
    const base::FilePath& entry_path,
    const fileapi::AsyncFileUtil::GetFileInfoCallback& callback) {
  // Return fake metadata for the root directory only.
  if (entry_path.AsUTF8Unsafe() != "/") {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(
            callback, base::File::FILE_ERROR_NOT_FOUND, base::File::Info()));
    return;
  }

  base::File::Info file_info;
  file_info.size = 0;
  file_info.is_directory = true;
  file_info.is_symbolic_link = false;
  base::Time last_modified_time;
  const bool result = base::Time::FromString("Thu Apr 24 00:46:52 UTC 2014",
                                             &last_modified_time);
  DCHECK(result);
  file_info.last_modified = last_modified_time;

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_OK, file_info));
}

const ProvidedFileSystemInfo& FakeProvidedFileSystem::GetFileSystemInfo()
    const {
  return file_system_info_;
}

RequestManager* FakeProvidedFileSystem::GetRequestManager() {
  NOTREACHED();
  return NULL;
}

ProvidedFileSystemInterface* FakeProvidedFileSystem::Create(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info) {
  return new FakeProvidedFileSystem(file_system_info);
}

}  // namespace file_system_provider
}  // namespace chromeos
