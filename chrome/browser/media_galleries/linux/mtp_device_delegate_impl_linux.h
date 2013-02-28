// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_DELEGATE_IMPL_LINUX_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_DELEGATE_IMPL_LINUX_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/media_galleries/mtp_device_delegate_impl.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/media/mtp_device_delegate.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace chrome {

// This class communicates with the MTP storage to complete the isolated file
// system operations. This class contains platform specific code to communicate
// with the attached MTP storage.
// Instantiate this class per MTP storage using CreateMTPDeviceDelegate().
// This object lives on a sequenced task runner thread, except for
// CancelPendingTasksAndDeleteDelegate() which happens on the UI thread.
class MTPDeviceDelegateImplLinux : public fileapi::MTPDeviceDelegate {
 private:
  friend void CreateMTPDeviceDelegate(const std::string&,
                                      base::SequencedTaskRunner*,
                                      const CreateMTPDeviceDelegateCallback&);
  friend class base::DeleteHelper<MTPDeviceDelegateImplLinux>;

  // Should only be called by CreateMTPDeviceDelegate() factory call.
  // Defer the device initializations until the first file operation request.
  // Do all the initializations in LazyInit() function.
  MTPDeviceDelegateImplLinux(const std::string& device_location,
                             base::SequencedTaskRunner* media_task_runner);

  // Destructed via CancelPendingTasksAndDeleteDelegate().
  virtual ~MTPDeviceDelegateImplLinux();

  // Opens the device for communication. This function is called on
  // |media_task_runner_|. Returns true if the device is ready for
  // communication, else false.
  bool LazyInit();

  // MTPDeviceDelegate:
  virtual base::PlatformFileError GetFileInfo(
      const base::FilePath& file_path,
      base::PlatformFileInfo* file_info) OVERRIDE;
  virtual scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
      CreateFileEnumerator(const base::FilePath& root,
                           bool recursive) OVERRIDE;
  virtual base::PlatformFileError CreateSnapshotFile(
      const base::FilePath& device_file_path,
      const base::FilePath& local_path,
      base::PlatformFileInfo* file_info) OVERRIDE;
  virtual void CancelPendingTasksAndDeleteDelegate() OVERRIDE;

  // Stores the registered file system device path value. This path does not
  // correspond to a real device path (E.g.: "/usb:2,2:81282").
  const std::string device_path_;

  // Stores the device handle returned by
  // MediaTransferProtocolManager::OpenStorage().
  std::string device_handle_;

  // Stores a reference to worker pool thread. All requests and response of file
  // operations are posted on |media_task_runner_|.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // |media_task_runner_| can wait on this event until the requested task is
  // complete.
  base::WaitableEvent on_task_completed_event_;

  // Used to notify |media_task_runner_| pending tasks about the shutdown
  // sequence. Signaled on the UI thread.
  base::WaitableEvent on_shutdown_event_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceDelegateImplLinux);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_DELEGATE_IMPL_LINUX_H_
