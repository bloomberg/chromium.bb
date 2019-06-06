// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_handle_base.h"

#include "content/public/common/content_switches.h"

namespace content {

NativeFileSystemHandleBase::NativeFileSystemHandleBase(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    storage::IsolatedContext::ScopedFSHandle file_system)
    : manager_(manager),
      context_(context),
      url_(url),
      file_system_(std::move(file_system)) {
  DCHECK(manager_);
  DCHECK_EQ(url_.mount_type() == storage::kFileSystemTypeIsolated,
            file_system_.is_valid())
      << url_.mount_type();
  // For now only support sandboxed file system and native file system.
  DCHECK(url_.type() == storage::kFileSystemTypeNativeLocal ||
         url_.type() == storage::kFileSystemTypePersistent ||
         url_.type() == storage::kFileSystemTypeTemporary ||
         url_.type() == storage::kFileSystemTypeTest)
      << url_.type();
  // Sandboxed and test file systems should always be writable.
  if (url_.type() == storage::kFileSystemTypePersistent ||
      url_.type() == storage::kFileSystemTypeTemporary ||
      url_.type() == storage::kFileSystemTypeTest) {
    write_permission_status_ = PermissionStatus::GRANTED;
  }
}

NativeFileSystemHandleBase::~NativeFileSystemHandleBase() = default;

NativeFileSystemHandleBase::PermissionStatus
NativeFileSystemHandleBase::GetWritePermissionStatus() const {
  // It is not currently possible to have write only handles, so first check the
  // read permission status. See also:
  // http://wicg.github.io/native-file-system/#api-filesystemhandle-querypermission
  if (read_permission_status_ != PermissionStatus::GRANTED)
    return read_permission_status_;
  return write_permission_status_;
}

void NativeFileSystemHandleBase::DoGetPermissionStatus(
    bool writable,
    base::OnceCallback<void(PermissionStatus)> callback) {
  std::move(callback).Run(writable ? GetWritePermissionStatus()
                                   : GetReadPermissionStatus());
}

void NativeFileSystemHandleBase::DoRequestPermission(
    bool writable,
    base::OnceCallback<void(PermissionStatus)> callback) {
  PermissionStatus current_status =
      writable ? GetWritePermissionStatus() : GetReadPermissionStatus();
  if (current_status != PermissionStatus::ASK) {
    std::move(callback).Run(current_status);
    return;
  }
  if (!writable) {
    // TODO(https://crbug.com/971401): Implement prompt. Currently unreachable,
    // since read permission isn't revokable yet.
    std::move(callback).Run(read_permission_status_);
    return;
  }
  // TODO(https://crbug.com/971401): Today read permission isn't revokable, so
  // current status should always be GRANTED.
  DCHECK_EQ(GetReadPermissionStatus(), PermissionStatus::GRANTED);
  // TODO(https://crbug.com/878585): Actually prompt for write permission. For
  // now we're just always denying requests, unless Experimental Web Platform
  // features are enabled.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    write_permission_status_ = PermissionStatus::GRANTED;
  } else {
    write_permission_status_ = PermissionStatus::DENIED;
  }
  std::move(callback).Run(write_permission_status_);
}

}  // namespace content
