// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_DEVICE_OBJECT_ENUMERATOR_H_
#define CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_DEVICE_OBJECT_ENUMERATOR_H_

#include <vector>

#include "base/file_path.h"
#include "base/time.h"
#include "device/media_transfer_protocol/mtp_file_entry.pb.h"
#include "webkit/fileapi/file_system_file_util.h"

namespace chrome {

// Used to enumerate top-level files of an media file system.
class MTPDeviceObjectEnumerator
    : public fileapi::FileSystemFileUtil::AbstractFileEnumerator {
 public:
  explicit MTPDeviceObjectEnumerator(const std::vector<MtpFileEntry>& entries);

  virtual ~MTPDeviceObjectEnumerator();

  // AbstractFileEnumerator:
  virtual FilePath Next() OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual bool IsDirectory() OVERRIDE;
  virtual base::Time LastModifiedTime() OVERRIDE;

  // If the current file entry is valid, returns true and fills in |entry_id|
  // with the entry identifier else returns false and |entry_id| is not set.
  bool GetEntryId(uint32_t* entry_id) const;

 private:
  // Returns true if the enumerator has more entries to traverse, false
  // otherwise.
  bool HasMoreEntries() const;

  // Returns true if Next() has been called at least once, and the enumerator
  // has more entries to traverse.
  bool IsIndexReadyAndInRange() const;

  // List of directory file entries information.
  const std::vector<MtpFileEntry> file_entries_;

  // Index into |file_entries_|.
  // Should only be used when |is_index_ready_| is true.
  size_t index_;

  // Initially false. Set to true after Next() has been called.
  bool is_index_ready_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceObjectEnumerator);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_DEVICE_OBJECT_ENUMERATOR_H_
