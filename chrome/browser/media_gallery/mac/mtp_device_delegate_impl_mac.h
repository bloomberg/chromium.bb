// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MTP_DEVICE_DELEGATE_IMPL_MAC_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MTP_DEVICE_DELEGATE_IMPL_MAC_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/media/mtp_device_delegate.h"

namespace base {
class SequencedTaskRunner;
}

namespace chrome {

// Delegate for presenting an Image Capture device through the filesystem
// API. The synthetic filesystem will be rooted at the constructed location,
// and names of all files notified through the ItemAdded call will be
// all appear as children of that directory. (ItemAdded calls with directories
// will be ignored.)
class MTPDeviceDelegateImplMac : public fileapi::MTPDeviceDelegate {
 public:
  MTPDeviceDelegateImplMac(const std::string& device_id,
                           const base::FilePath::StringType& synthetic_path,
                           base::SequencedTaskRunner* media_task_runner);

  // MTPDeviceDelegate:
  virtual base::PlatformFileError GetFileInfo(
      const base::FilePath& file_path,
      base::PlatformFileInfo* file_info) OVERRIDE;
  virtual scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
      CreateFileEnumerator(const base::FilePath& root, bool recursive) OVERRIDE;
  virtual base::PlatformFileError CreateSnapshotFile(
      const base::FilePath& device_file_path,
      const base::FilePath& local_path,
      base::PlatformFileInfo* file_info) OVERRIDE;
  virtual void CancelPendingTasksAndDeleteDelegate() OVERRIDE;

  // Forward delegates for ImageCaptureDeviceListener
  virtual void ItemAdded(const std::string& name,
                         const base::PlatformFileInfo& info);
  virtual void NoMoreItems();
  virtual void DownloadedFile(const std::string& name,
                              base::PlatformFileError error);

  // Implementation returned by |CreateFileEnumerator()|. Must be deleted
  // before CancelPendingTasksAndDeleteDelegate is called.
  class Enumerator :
      public fileapi::FileSystemFileUtil::AbstractFileEnumerator {
   public:
    explicit Enumerator(MTPDeviceDelegateImplMac* delegate);
    virtual ~Enumerator();

    virtual base::FilePath Next() OVERRIDE;
    virtual int64 Size() OVERRIDE;
    virtual base::Time LastModifiedTime() OVERRIDE;
    virtual bool IsDirectory() OVERRIDE;

    // Called by the delegate to signal any waiters that the received items
    // list has changed state.
    void ItemsChanged();

   private:
    MTPDeviceDelegateImplMac* delegate_;
    size_t position_;
    base::WaitableEvent wait_for_items_;
  };

  // GetFile and HasAllFiles called by enumerators.
  base::FilePath GetFile(size_t index);
  bool ReceivedAllFiles();
  void RemoveEnumerator(Enumerator* enumerator);

 private:
  friend class base::DeleteHelper<MTPDeviceDelegateImplMac>;

  class DeviceListener;

  virtual ~MTPDeviceDelegateImplMac();

  std::string device_id_;
  base::FilePath root_path_;

  // Stores a reference to worker pool thread. All requests and response of file
  // operations are posted on |media_task_runner_|. All callbacks from the
  // camera will come through this task runner as well.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // Interface object for the camera underlying this MTP session.
  scoped_ptr<DeviceListener> camera_interface_;

  // This lock protects all subsequent state. While calling into the delegate
  // from the FileSystem API happens in sequence, these calls may be
  // interleaved with calls on other threads in the sequenced task runner
  // forwarded from the device.
  base::Lock mutex_;

  // Weak pointer to the enumerator which may be listening for files to come in.
  Enumerator* enumerator_;

  // Stores a map from filename to file metadata received from the camera.
  base::hash_map<base::FilePath::StringType, base::PlatformFileInfo> file_info_;

  // Notification for pending download.
  base::WaitableEvent* file_download_event_;

  // Resulting error code for pending download.
  base::PlatformFileError file_download_error_;

  // List of files received from the camera.
  std::vector<base::FilePath> file_paths_;

  // Set to true when all file metadata has been received from the camera.
  bool received_all_files_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceDelegateImplMac);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MTP_DEVICE_DELEGATE_IMPL_MAC_H_
