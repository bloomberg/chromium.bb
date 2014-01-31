// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_SCAN_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_SCAN_MANAGER_OBSERVER_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/media_galleries/media_scan_types.h"

class MediaScanManagerObserver {
 public:
  virtual void OnScanStarted(const std::string& extension_id) {}
  virtual void OnScanCancelled(const std::string& extension_id) {}
  virtual void OnScanFinished(
      const std::string& extension_id,
      int gallery_count,
      const MediaGalleryScanResult& file_counts) {}
  virtual void OnScanError(const std::string& extension_id) {}
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_SCAN_MANAGER_OBSERVER_H_
