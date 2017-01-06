// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cros_disks_client.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

namespace {

// Performs fake mounting by creating a directory with a dummy file.
MountError PerformFakeMount(const std::string& source_path,
                            const base::FilePath& mounted_path) {
  // Just create an empty directory and shows it as the mounted directory.
  if (!base::CreateDirectory(mounted_path)) {
    DLOG(ERROR) << "Failed to create directory at " << mounted_path.value();
    return MOUNT_ERROR_DIRECTORY_CREATION_FAILED;
  }

  // Put a dummy file.
  const base::FilePath dummy_file_path =
      mounted_path.Append("SUCCESSFULLY_PERFORMED_FAKE_MOUNT.txt");
  const std::string dummy_file_content = "This is a dummy file.";
  const int write_result = base::WriteFile(
      dummy_file_path, dummy_file_content.data(), dummy_file_content.size());
  if (write_result != static_cast<int>(dummy_file_content.size())) {
    DLOG(ERROR) << "Failed to put a dummy file at "
                << dummy_file_path.value();
    return MOUNT_ERROR_MOUNT_PROGRAM_FAILED;
  }

  return MOUNT_ERROR_NONE;
}

// Continuation of Mount().
void DidMount(const CrosDisksClient::MountCompletedHandler&
              mount_completed_handler,
              const std::string& source_path,
              MountType type,
              const base::Closure& callback,
              const base::FilePath& mounted_path,
              MountError mount_error) {
  // Tell the caller of Mount() that the mount request was accepted.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);

  // Tell the caller of Mount() that the mount completed.
  if (!mount_completed_handler.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(mount_completed_handler,
                              MountEntry(mount_error, source_path, type,
                                         mounted_path.AsUTF8Unsafe())));
  }
}

}  // namespace

FakeCrosDisksClient::FakeCrosDisksClient()
    : unmount_call_count_(0),
      unmount_success_(true),
      format_call_count_(0),
      format_success_(true) {
}

FakeCrosDisksClient::~FakeCrosDisksClient() {
}

void FakeCrosDisksClient::Init(dbus::Bus* bus) {
}

void FakeCrosDisksClient::Mount(const std::string& source_path,
                                const std::string& source_format,
                                const std::string& mount_label,
                                MountAccessMode access_mode,
                                RemountOption remount,
                                const base::Closure& callback,
                                const base::Closure& error_callback) {
  // This fake implementation assumes mounted path is device when source_format
  // is empty, or an archive otherwise.
  MountType type =
      (source_format == "") ? MOUNT_TYPE_DEVICE : MOUNT_TYPE_ARCHIVE;

  base::FilePath mounted_path;
  switch (type) {
    case MOUNT_TYPE_ARCHIVE:
      mounted_path = GetArchiveMountPoint().Append(
          base::FilePath::FromUTF8Unsafe(mount_label));
      break;
    case MOUNT_TYPE_DEVICE:
      mounted_path = GetRemovableDiskMountPoint().Append(
          base::FilePath::FromUTF8Unsafe(mount_label));
      break;
    case MOUNT_TYPE_INVALID:
      // Unreachable
      return;
  }
  mounted_paths_.insert(mounted_path);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits()
                     .WithShutdownBehavior(
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
                     .MayBlock(),
      base::Bind(&PerformFakeMount, source_path, mounted_path),
      base::Bind(&DidMount, mount_completed_handler_, source_path, type,
                 callback, mounted_path));
}

void FakeCrosDisksClient::Unmount(const std::string& device_path,
                                  UnmountOptions options,
                                  const base::Closure& callback,
                                  const base::Closure& error_callback) {
  DCHECK(!callback.is_null());
  DCHECK(!error_callback.is_null());

  // Remove the dummy mounted directory if it exists.
  if (mounted_paths_.count(base::FilePath::FromUTF8Unsafe(device_path)) > 0) {
    mounted_paths_.erase(base::FilePath::FromUTF8Unsafe(device_path));
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, base::TaskTraits()
                       .WithShutdownBehavior(
                           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
                       .WithPriority(base::TaskPriority::BACKGROUND)
                       .MayBlock(),
        base::Bind(base::IgnoreResult(&base::DeleteFile),
                   base::FilePath::FromUTF8Unsafe(device_path),
                   true /* recursive */),
        callback);
  }

  unmount_call_count_++;
  last_unmount_device_path_ = device_path;
  last_unmount_options_ = options;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, unmount_success_ ? callback : error_callback);
  if (!unmount_listener_.is_null())
    unmount_listener_.Run();
}

void FakeCrosDisksClient::EnumerateAutoMountableDevices(
    const EnumerateAutoMountableDevicesCallback& callback,
    const base::Closure& error_callback) {
}

void FakeCrosDisksClient::EnumerateMountEntries(
    const EnumerateMountEntriesCallback& callback,
    const base::Closure& error_callback) {
}

void FakeCrosDisksClient::Format(const std::string& device_path,
                                 const std::string& filesystem,
                                 const base::Closure& callback,
                                 const base::Closure& error_callback) {
  DCHECK(!callback.is_null());
  DCHECK(!error_callback.is_null());

  format_call_count_++;
  last_format_device_path_ = device_path;
  last_format_filesystem_ = filesystem;
  if (format_success_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, error_callback);
  }
}

void FakeCrosDisksClient::GetDeviceProperties(
    const std::string& device_path,
    const GetDevicePropertiesCallback& callback,
    const base::Closure& error_callback) {
}

void FakeCrosDisksClient::SetMountEventHandler(
    const MountEventHandler& mount_event_handler) {
  mount_event_handler_ = mount_event_handler;
}

void FakeCrosDisksClient::SetMountCompletedHandler(
    const MountCompletedHandler& mount_completed_handler) {
  mount_completed_handler_ = mount_completed_handler;
}

void FakeCrosDisksClient::SetFormatCompletedHandler(
    const FormatCompletedHandler& format_completed_handler) {
  format_completed_handler_ = format_completed_handler;
}

bool FakeCrosDisksClient::SendMountEvent(MountEventType event,
                                         const std::string& path) {
  if (mount_event_handler_.is_null())
    return false;
  mount_event_handler_.Run(event, path);
  return true;
}

bool FakeCrosDisksClient::SendMountCompletedEvent(
    MountError error_code,
    const std::string& source_path,
    MountType mount_type,
    const std::string& mount_path) {
  if (mount_completed_handler_.is_null())
    return false;
  mount_completed_handler_.Run(
      MountEntry(error_code, source_path, mount_type, mount_path));
  return true;
}

bool FakeCrosDisksClient::SendFormatCompletedEvent(
    FormatError error_code,
    const std::string& device_path) {
  if (format_completed_handler_.is_null())
    return false;
  format_completed_handler_.Run(error_code, device_path);
  return true;
}

}  // namespace chromeos
