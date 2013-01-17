// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class enumerates the media transfer protocol (MTP) device objects from
// a given object entry list.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_WIN_MTP_DEVICE_OBJECT_ENUMERATOR_H_
#define CHROME_BROWSER_MEDIA_GALLERY_WIN_MTP_DEVICE_OBJECT_ENUMERATOR_H_

#include "base/file_path.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "chrome/browser/media_gallery/win/mtp_device_object_entry.h"
#include "webkit/fileapi/file_system_file_util.h"

namespace chrome {

// MTPDeviceObjectEnumerator is used to enumerate the media transfer protocol
// (MTP) device objects from a given object entry list.
// MTPDeviceObjectEnumerator supports MTP device file operations.
// MTPDeviceObjectEnumerator may only be used on a single thread.
class MTPDeviceObjectEnumerator
    : public fileapi::FileSystemFileUtil::AbstractFileEnumerator {
 public:
  explicit MTPDeviceObjectEnumerator(const MTPDeviceObjectEntries& entries);
  virtual ~MTPDeviceObjectEnumerator();

  // AbstractFileEnumerator:
  virtual FilePath Next() OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual bool IsDirectory() OVERRIDE;
  virtual base::Time LastModifiedTime() OVERRIDE;

 private:
  // The directory file object entries.
  MTPDeviceObjectEntries object_entries_;

  // Iterator to access the individual file object entries.
  MTPDeviceObjectEntries::const_iterator object_entry_iter_;

  // The current file object entry.
  const MTPDeviceObjectEntry* current_object_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceObjectEnumerator);
};

}  // namepsace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_WIN_MTP_DEVICE_OBJECT_ENUMERATOR_H_
