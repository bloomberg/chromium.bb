// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_SCOPED_MTP_DEVICE_MAP_ENTRY_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_SCOPED_MTP_DEVICE_MAP_ENTRY_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/public/browser/browser_thread.h"

class MediaFileSystemContext;
class MTPDeviceAsyncDelegate;

// ScopedMTPDeviceMapEntry manages the reference count on a particular
// MTP device location. These objects are held reference counted in
// the ExtensionGalleriesHost objects. When a particular location is
// destroyed, the MediaFileSystemContext is notified, and the attendant
// delegates are subsequently erased from the system-wide MTPServiceMap.
class ScopedMTPDeviceMapEntry
    : public base::RefCountedThreadSafe<
          ScopedMTPDeviceMapEntry, content::BrowserThread::DeleteOnUIThread> {
 public:
  // Created on the UI thread.
  ScopedMTPDeviceMapEntry(const base::FilePath::StringType& device_location,
                          MediaFileSystemContext* context);

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

  // Notified when the object is destroyed.
  MediaFileSystemContext* context_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMTPDeviceMapEntry);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_SCOPED_MTP_DEVICE_MAP_ENTRY_H_
