// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CROS_DISKS_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CROS_DISKS_CLIENT_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chromeos/dbus/cros_disks_client.h"

namespace chromeos {

// A fake implementation of CrosDiskeClient. This class provides a fake behavior
// and the user of this class can raise a fake mouse events.
class CHROMEOS_EXPORT FakeCrosDisksClient : public CrosDisksClient {
 public:
  FakeCrosDisksClient();
  ~FakeCrosDisksClient() override;

  // CrosDisksClient overrides
  void Init(dbus::Bus* bus) override;

  // Performs fake mounting for archive files. Instead of actually extracting
  // contents of archive files, this function creates a directory that
  // contains a dummy file.
  void Mount(const std::string& source_path,
             const std::string& source_format,
             const std::string& mount_label,
             MountAccessMode access_mode,
             RemountOption remount,
             const base::Closure& callback,
             const base::Closure& error_callback) override;

  // Deletes the directory created in Mount().
  void Unmount(const std::string& device_path,
               UnmountOptions options,
               const base::Closure& callback,
               const base::Closure& error_callback) override;
  void EnumerateAutoMountableDevices(
      const EnumerateAutoMountableDevicesCallback& callback,
      const base::Closure& error_callback) override;
  void EnumerateMountEntries(const EnumerateMountEntriesCallback& callback,
                             const base::Closure& error_callback) override;
  void Format(const std::string& device_path,
              const std::string& filesystem,
              const base::Closure& callback,
              const base::Closure& error_callback) override;
  void Rename(const std::string& device_path,
              const std::string& volume_name,
              const base::Closure& callback,
              const base::Closure& error_callback) override;
  void GetDeviceProperties(const std::string& device_path,
                           const GetDevicePropertiesCallback& callback,
                           const base::Closure& error_callback) override;
  void SetMountEventHandler(
      const MountEventHandler& mount_event_handler) override;
  void SetMountCompletedHandler(
      const MountCompletedHandler& mount_completed_handler) override;
  void SetFormatCompletedHandler(
      const FormatCompletedHandler& format_completed_handler) override;
  void SetRenameCompletedHandler(
      const RenameCompletedHandler& rename_completed_handler) override;

  // Used in tests to simulate signals sent by cros disks layer.
  // Invokes handlers set in |SetMountEventHandler|, |SetMountCompletedHandler|,
  // and |SetFormatCompletedHandler|.
  bool SendMountEvent(MountEventType event, const std::string& path);
  bool SendMountCompletedEvent(MountError error_code,
                               const std::string& source_path,
                               MountType mount_type,
                               const std::string& mount_path);
  bool SendFormatCompletedEvent(FormatError error_code,
                                const std::string& device_path);
  bool SendRenameCompletedEvent(RenameError error_code,
                                const std::string& device_path);

  // Returns how many times Unmount() was called.
  int unmount_call_count() const {
    return unmount_call_count_;
  }

  // Returns the |device_path| parameter from the last invocation of Unmount().
  const std::string& last_unmount_device_path() const {
    return last_unmount_device_path_;
  }

  // Returns the |options| parameter from the last invocation of Unmount().
  UnmountOptions last_unmount_options() const {
    return last_unmount_options_;
  }

  // Makes the subsequent Unmount() calls fail. Unmount() succeeds by default.
  void MakeUnmountFail() {
    unmount_success_ = false;
  }

  // Sets a listener callbackif the following Unmount() call is success or not.
  // Unmount() calls the corresponding callback given as a parameter.
  void set_unmount_listener(base::Closure listener) {
    unmount_listener_ = listener;
  }

  // Returns how many times Format() was called.
  int format_call_count() const {
    return format_call_count_;
  }

  // Returns the |device_path| parameter from the last invocation of Format().
  const std::string& last_format_device_path() const {
    return last_format_device_path_;
  }

  // Returns the |filesystem| parameter from the last invocation of Format().
  const std::string& last_format_filesystem() const {
    return last_format_filesystem_;
  }

  // Makes the subsequent Format() calls fail. Format() succeeds by default.
  void MakeFormatFail() {
    format_success_ = false;
  }

  // Returns how many times Rename() was called.
  int rename_call_count() const { return rename_call_count_; }

  // Returns the |device_path| parameter from the last invocation of Rename().
  const std::string& last_rename_device_path() const {
    return last_rename_device_path_;
  }

  // Returns the |volume_name| parameter from the last invocation of Rename().
  const std::string& last_rename_volume_name() const {
    return last_rename_volume_name_;
  }

  // Makes the subsequent Rename() calls fail. Rename() succeeds by default.
  void MakeRenameFail() { rename_success_ = false; }

 private:
  MountEventHandler mount_event_handler_;
  MountCompletedHandler mount_completed_handler_;
  FormatCompletedHandler format_completed_handler_;
  RenameCompletedHandler rename_completed_handler_;

  int unmount_call_count_;
  std::string last_unmount_device_path_;
  UnmountOptions last_unmount_options_;
  bool unmount_success_;
  base::Closure unmount_listener_;
  int format_call_count_;
  std::string last_format_device_path_;
  std::string last_format_filesystem_;
  bool format_success_;
  int rename_call_count_;
  std::string last_rename_device_path_;
  std::string last_rename_volume_name_;
  bool rename_success_;
  std::set<base::FilePath> mounted_paths_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CROS_DISKS_CLIENT_H_
