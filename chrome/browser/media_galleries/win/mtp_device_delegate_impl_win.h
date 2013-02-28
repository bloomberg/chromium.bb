// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_DELEGATE_IMPL_WIN_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_DELEGATE_IMPL_WIN_H_

#include <portabledeviceapi.h>

#include <string>

#include "base/platform_file.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "base/win/scoped_comptr.h"
#include "chrome/browser/media_galleries/mtp_device_delegate_impl.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/media/mtp_device_delegate.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace chrome {

// MTPDeviceDelegateImplWin is used to communicate with the media transfer
// protocol (MTP) device to complete isolated file system operations. MTP
// device can have multiple data storage partitions. MTPDeviceDelegateImplWin
// is instantiated per MTP device storage partition using
// CreateMTPDeviceDelegate(). MTPDeviceDelegateImplWin lives on a sequenced
// task runner thread, except for CancelPendingTasksAndDeleteDelegate() which
// happens on the UI thread.
class MTPDeviceDelegateImplWin : public fileapi::MTPDeviceDelegate {
 private:
  friend void OnGotStorageInfoCreateDelegate(
      const string16& device_location,
      base::SequencedTaskRunner* media_task_runner,
      const CreateMTPDeviceDelegateCallback& callback,
      const string16& pnp_device_id,
      const string16& storage_object_id);

  friend class base::DeleteHelper<MTPDeviceDelegateImplWin>;

  // Defers the device initializations until the first file operation request.
  // Do all the initializations in LazyInit() function.
  MTPDeviceDelegateImplWin(const string16& fs_root_path,
                           const string16& pnp_device_id,
                           const string16& storage_object_id,
                           base::SequencedTaskRunner* media_task_runner);

  // Destructed via CancelPendingTasksAndDeleteDelegate().
  virtual ~MTPDeviceDelegateImplWin();

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

  // Returns whether the device is ready for communication.
  bool LazyInit();

  // Returns the object id of the file object specified by the |file_path|,
  // e.g. if the |file_path| is "\\MTP:StorageSerial:SID-{1001,,192}:125\DCIM"
  // and |registered_device_path_| is "\\MTP:StorageSerial:SID-{1001,,192}:125",
  // this function returns the identifier of the "DCIM" folder object.
  //
  // Returns an empty string if the device is detached while the request is in
  // progress.
  string16 GetFileObjectIdFromPath(const string16& file_path);

  // The PnP Device Id, used to open the device for communication,
  // e.g. "\\?\usb#vid_04a9&pid_3073#12#{6ac27878-a6fa-4155-ba85-f1d4f33}".
  const string16 pnp_device_id_;

  // The media file system root path, which is obtained during the registration
  // of MTP device storage partition as a file system,
  // e.g. "\\MTP:StorageSerial:SID-{10001,E,9823}:237483".
  const string16 registered_device_path_;

  // The MTP device storage partition object identifier, used to enumerate the
  // storage contents, e.g. "s10001".
  const string16 storage_object_id_;

  // The task runner where most of the class lives and runs (save for
  // CancelPendingTasksAndDeleteDelegate).
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // The portable device.
  base::win::ScopedComPtr<IPortableDevice> device_;

  // The lock to protect |cancel_pending_tasks_|. |cancel_pending_tasks_| is
  // set on the UI thread when the
  // (1) Browser application is in shutdown mode (or)
  // (2) Last extension using this MTP device is destroyed (or)
  // (3) Attached MTP device is removed (or)
  // (4) User revoked the MTP device gallery permission.
  base::Lock cancel_tasks_lock_;
  bool cancel_pending_tasks_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceDelegateImplWin);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_DELEGATE_IMPL_WIN_H_
