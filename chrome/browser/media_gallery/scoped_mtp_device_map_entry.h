// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Class to manage the lifetime of MTPDeviceDelegateImpl object for the
// attached media transfer protocol (MTP) device. This object is constructed
// for each MTP device. Refcounted to reuse the same MTP device delegate entry
// across extensions.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_SCOPED_MTP_DEVICE_MAP_ENTRY_H_
#define CHROME_BROWSER_MEDIA_GALLERY_SCOPED_MTP_DEVICE_MAP_ENTRY_H_

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"

namespace chrome {

class ScopedMTPDeviceMapEntry
    : public base::RefCounted<ScopedMTPDeviceMapEntry> {
 public:
  // |no_references_callback| is called when the last ScopedMTPDeviceMapEntry
  // reference goes away.
  ScopedMTPDeviceMapEntry(const FilePath::StringType& device_location,
                          const base::Closure& no_references_callback);

 private:
  // Friend declaration for ref counted implementation.
  friend class base::RefCounted<ScopedMTPDeviceMapEntry>;

  // Private because this class is ref-counted. Destructed when the last user of
  // this MTP device is destroyed or when the MTP device is detached from the
  // system or when the browser is in shutdown mode or when the last extension
  // revokes the MTP device gallery permissions.
  ~ScopedMTPDeviceMapEntry();

  // The MTP or PTP device location.
  const FilePath::StringType device_location_;

  // A callback to call when the last reference of this object goes away.
  base::Closure no_references_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMTPDeviceMapEntry);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_SCOPED_MTP_DEVICE_MAP_ENTRY_H_
