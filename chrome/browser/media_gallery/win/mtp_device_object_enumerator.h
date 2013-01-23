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

  // If the current file object entry is valid, returns an non-empty object id.
  // Returns an empty string otherwise.
  string16 GetObjectId() const;

 private:
  // Returns true if the enumerator has more entries to traverse, false
  // otherwise.
  bool HasMoreEntries() const;

  // Returns true if Next() has been called at least once, and the enumerator
  // has more entries to traverse.
  bool IsIndexReadyAndInRange() const;

  // List of directory file object entries.
  MTPDeviceObjectEntries object_entries_;

  // Index into |object_entries_|.
  // Should only be used when |is_index_ready_| is true.
  size_t index_;

  // Initially false. Set to true after Next() has been called.
  bool is_index_ready_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceObjectEnumerator);
};

}  // namepsace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_WIN_MTP_DEVICE_OBJECT_ENUMERATOR_H_
