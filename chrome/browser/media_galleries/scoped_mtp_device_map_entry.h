// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_SCOPED_MTP_DEVICE_MAP_ENTRY_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_SCOPED_MTP_DEVICE_MAP_ENTRY_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

class MTPDeviceAsyncDelegate;

// ScopedMTPDeviceMapEntry manages the reference count on a particular
// MTP device location. These objects are held reference counted in
// the ExtensionGalleriesHost objects. When a particular location is
// destroyed, the constructor-time callback tells the MediaFileSystemRegistry
// to erase it from the system-wide map, and it is also removed from
// the MTPServiceMap at that point.
// TODO(gbillock): Move this to media_file_system_registry.
class ScopedMTPDeviceMapEntry
    : public base::RefCountedThreadSafe<
          ScopedMTPDeviceMapEntry, content::BrowserThread::DeleteOnUIThread> {
 public:
  // |on_destruction_callback| is called when ScopedMTPDeviceMapEntry gets
  // destroyed.
  // Created on the UI thread.
  ScopedMTPDeviceMapEntry(const base::FilePath::StringType& device_location,
                          const base::Closure& on_destruction_callback);

 private:
  // Friend declarations for ref counted implementation.
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<ScopedMTPDeviceMapEntry>;

  // Private because this class is ref-counted. Destroyed when:
  // - no extension is using the device.
  // - no extension has permission to access to device.
  // - the device is detached.
  // - the browser shuts down.
  // Destroyed on the UI thread.
  ~ScopedMTPDeviceMapEntry();

  // The MTP or PTP device location.
  const base::FilePath::StringType device_location_;

  // Called when the object is destroyed.
  base::Closure on_destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMTPDeviceMapEntry);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_SCOPED_MTP_DEVICE_MAP_ENTRY_H_
