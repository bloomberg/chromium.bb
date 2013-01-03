// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ScopedMTPDeviceMapEntry manages the lifetime of a MTPDeviceDelegate.
// Each extension that uses a device holds a reference to the device's
// ScopedMTPDeviceMapEntry.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_SCOPED_MTP_DEVICE_MAP_ENTRY_H_
#define CHROME_BROWSER_MEDIA_GALLERY_SCOPED_MTP_DEVICE_MAP_ENTRY_H_

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/public/browser/browser_thread.h"

namespace fileapi {
class MTPDeviceDelegate;
}

namespace chrome {

class ScopedMTPDeviceMapEntry
    : public base::RefCountedThreadSafe<
          ScopedMTPDeviceMapEntry, content::BrowserThread::DeleteOnUIThread> {
 public:
  // |on_destruction_callback| is called when ScopedMTPDeviceMapEntry gets
  // destroyed.
  // Created on the UI thread.
  ScopedMTPDeviceMapEntry(const FilePath::StringType& device_location,
                          const base::Closure& on_destruction_callback);

  // Most be called after creating the ScopedMTPDeviceMapEntry.
  void Init();

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

  // Callback to add the managed MTPDeviceDelegate to the MTPDeviceMapService.
  // Called on the media task runner thread.
  void OnMTPDeviceDelegateCreated(fileapi::MTPDeviceDelegate* delegate);

  // The MTP or PTP device location.
  const FilePath::StringType device_location_;

  // Called when the object is destroyed.
  base::Closure on_destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMTPDeviceMapEntry);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_SCOPED_MTP_DEVICE_MAP_ENTRY_H_
