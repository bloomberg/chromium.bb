// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"

#include "base/files/file.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "extensions/browser/event_router.h"

namespace chromeos {
namespace file_system_provider {

FakeProvidedFileSystem::FakeProvidedFileSystem(
    const ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info) {}

FakeProvidedFileSystem::~FakeProvidedFileSystem() {}

bool FakeProvidedFileSystem::RequestUnmount(
    const fileapi::AsyncFileUtil::StatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_OK));
  return true;
}

const ProvidedFileSystemInfo& FakeProvidedFileSystem::GetFileSystemInfo()
    const {
  return file_system_info_;
}

ProvidedFileSystemInterface* FakeProvidedFileSystem::Create(
    extensions::EventRouter* event_router,
    RequestManager* request_manager,
    const ProvidedFileSystemInfo& file_system_info) {
  // TODO(mtomasz): Create a request manager in ProvidedFileSystem, since it is
  // only used by ProvidedFileSystem, instead of having a profile wide one.
  // As a result, there will be no need to pass the request manager to the
  // factory callback.
  return new FakeProvidedFileSystem(file_system_info);
}

}  // namespace file_system_provider
}  // namespace chromeos
