// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_RECURSIVE_DEVICE_OBJECT_ENUMERATOR_H_
#define CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_RECURSIVE_DEVICE_OBJECT_ENUMERATOR_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "device/media_transfer_protocol/mtp_file_entry.pb.h"
#include "webkit/fileapi/file_system_file_util.h"

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}

namespace chrome {

// Recursively enumerate each file entry from a given media file entry set.
class MTPRecursiveDeviceObjectEnumerator
    : public fileapi::FileSystemFileUtil::AbstractFileEnumerator {
 public:
  MTPRecursiveDeviceObjectEnumerator(const std::string& handle,
                                     base::SequencedTaskRunner* task_runner,
                                     const std::vector<MtpFileEntry>& entries,
                                     base::WaitableEvent* task_completed_event,
                                     base::WaitableEvent* shutdown_event);

  virtual ~MTPRecursiveDeviceObjectEnumerator();

  // AbstractFileEnumerator:
  virtual FilePath Next() OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual bool IsDirectory() OVERRIDE;
  virtual base::Time LastModifiedTime() OVERRIDE;

 private:
  // Stores the device handle that was used to open the device.
  const std::string device_handle_;

  // Stores a reference to |media_task_runner_| to construct and destruct
  // ReadMTPDirectoryWorker object on the correct thread.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // List of top-level directory file entries.
  const std::vector<MtpFileEntry> file_entries_;

  // Iterator to access the individual file entries.
  std::vector<MtpFileEntry>::const_iterator file_entry_iter_;

  // Enumerator to access current directory Id/path entries.
  scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
      current_enumerator_;

  // |media_task_runner_| can wait on this event until the requested operation
  // is complete.
  base::WaitableEvent* on_task_completed_event_;

  // Stores a reference to waitable event associated with the shut down message.
  base::WaitableEvent* on_shutdown_event_;

  DISALLOW_COPY_AND_ASSIGN(MTPRecursiveDeviceObjectEnumerator);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_RECURSIVE_DEVICE_OBJECT_ENUMERATOR_H_
