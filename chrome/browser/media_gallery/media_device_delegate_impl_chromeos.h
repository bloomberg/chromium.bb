// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_DELEGATE_IMPL_CHROMEOS_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_DELEGATE_IMPL_CHROMEOS_H_

#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/media/media_device_delegate.h"

namespace base {
class SequencedTaskRunner;
}

class FilePath;

namespace chromeos {

// Helper class to communicate with MTP storage to complete isolated file system
// operations. This class contains platform specific code to communicate with
// the attached MTP storage. Instantiate this class per MTP storage.
// This class is ref-counted, because MediaDeviceDelegate is ref-counted.
class MediaDeviceDelegateImplCros : public fileapi::MediaDeviceDelegate {
 public:
  // Constructed on UI thread. Defer the device initializations until the first
  // file operation request. Do all the initializations in LazyInit() function.
  explicit MediaDeviceDelegateImplCros(const std::string& device_location);

  // Overridden from MediaDeviceDelegate. All the functions are called on
  // |media_task_runner_|.
  virtual base::PlatformFileError GetFileInfo(
      const FilePath& file_path,
      base::PlatformFileInfo* file_info) OVERRIDE;
  virtual fileapi::FileSystemFileUtil::AbstractFileEnumerator*
      CreateFileEnumerator(const FilePath& root, bool recursive) OVERRIDE;
  virtual base::PlatformFileError CreateSnapshotFile(
      const FilePath& device_file_path,
      const FilePath& local_path,
      base::PlatformFileInfo* file_info) OVERRIDE;
  virtual base::SequencedTaskRunner* media_task_runner() OVERRIDE;
  virtual void DeleteOnCorrectThread() const OVERRIDE;

 private:
  friend struct fileapi::MediaDeviceDelegateDeleter;
  friend class base::DeleteHelper<MediaDeviceDelegateImplCros>;

  // Private because this class is ref-counted.
  virtual ~MediaDeviceDelegateImplCros();

  // Opens the device for communication. This function is called on
  // |media_task_runner_|. Returns true if the device is ready for
  // communication, else false.
  bool LazyInit();

  // Stores the registered file system device path value. This path does not
  // correspond to a real device path (E.g.: "/usb:2,2:81282").
  const std::string device_path_;

  // Stores the device handle returned by
  // MediaTransferProtocolManager::OpenStorage().
  std::string device_handle_;

  // Stores a reference to worker pool thread. All requests and response of file
  // operations are posted on |media_task_runner_|.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MediaDeviceDelegateImplCros);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_DELEGATE_IMPL_CHROMEOS_H_
