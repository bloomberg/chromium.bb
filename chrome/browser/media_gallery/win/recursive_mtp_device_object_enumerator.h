// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_WIN_RECURSIVE_MTP_DEVICE_OBJECT_ENUMERATOR_H_
#define CHROME_BROWSER_MEDIA_GALLERY_WIN_RECURSIVE_MTP_DEVICE_OBJECT_ENUMERATOR_H_

#include <portabledeviceapi.h>

#include <queue>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "base/win/scoped_comptr.h"
#include "chrome/browser/media_gallery/win/mtp_device_object_entry.h"
#include "webkit/fileapi/file_system_file_util.h"

class FilePath;

namespace chrome {

// RecursiveMTPDeviceObjectEnumerator is used to recursively enumerate the
// media transfer protocol (MTP) device storage objects from a given media file
// object entries set. RecursiveMTPDeviceObjectEnumerator communicates with the
// MTP device to get the subdirectory object entries.
// RecursiveMTPDeviceObjectEnumerator uses MTPDeviceObjectEnumerator to iterate
// the current directory objects. RecursiveMTPDeviceObjectEnumerator may only be
// used on a single thread.
class RecursiveMTPDeviceObjectEnumerator
    : public fileapi::FileSystemFileUtil::AbstractFileEnumerator {
 public:
  // MTP |device| should already be initialized and ready for communication.
  RecursiveMTPDeviceObjectEnumerator(IPortableDevice* device,
                                     const MTPDeviceObjectEntries& entries);
  virtual ~RecursiveMTPDeviceObjectEnumerator();

  // AbstractFileEnumerator:
  virtual FilePath Next() OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual bool IsDirectory() OVERRIDE;
  virtual base::Time LastModifiedTime() OVERRIDE;

 private:
  typedef string16 DirectoryObjectId;

  void MaybeUpdateCurrentObjectList();

  // The portable device.
  base::win::ScopedComPtr<IPortableDevice> device_;

  // TODO(kmadhusu): Remove |curr_object_entries_| and |object_entry_iter_|.
  // Implement GetObjectId() and HasMoreEntries() in MTPDeviceObjectEnumerator
  // and remove |curr_object_entries_| and |object_entry_iter_|.

  // List of current directory object entries.
  MTPDeviceObjectEntries curr_object_entries_;

  // Iterator to access the individual file object entries.
  MTPDeviceObjectEntries::const_iterator object_entry_iter_;

  // Enumerator to access current directory object entries.
  scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
      current_enumerator_;

  // Used to recursively enumerate the sub-directory objects.
  std::queue<DirectoryObjectId> unparsed_directory_object_ids_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(RecursiveMTPDeviceObjectEnumerator);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_WIN_RECURSIVE_MTP_DEVICE_OBJECT_ENUMERATOR_H_
