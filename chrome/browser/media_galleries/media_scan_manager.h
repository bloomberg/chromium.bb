// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_SCAN_MANAGER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_SCAN_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

class Profile;
class MediaScanManagerObserver;

// The MediaScanManager is owned by MediaFileSystemRegistry, which is global.
// This class manages multiple 'virtual' media scans, up to one per extension
// per profile, and also manages the one physical scan backing them.
// This class lives and is called on the UI thread.
class MediaScanManager {
 public:
  MediaScanManager();
  virtual ~MediaScanManager();

  // There can only be ever one observer registered per profile. Does not take
  // ownership of |observer|. An observer must be registered before scanning.
  void AddObserver(Profile* profile, MediaScanManagerObserver* observer);
  void RemoveObserver(Profile* profile);

  // Must be called when |profile| is shut down.
  void CancelScansForProfile(Profile* profile);

  // The results of the scan are reported to the registered
  // MediaScanManagerObserver via OnScanFinished. There must be an observer
  // registered for |profile| before the scan starts.
  void StartScan(Profile* profile, const std::string& extension_id);
  void CancelScan(Profile* profile, const std::string& extension_id);

 private:
  typedef std::map<Profile*, std::set<std::string> > ScanningExtensionsMap;
  typedef std::map<Profile*, MediaScanManagerObserver*> ObserverMap;

  // Set of extensions (on all profiles) that have an in-progress scan.
  ScanningExtensionsMap scanning_extensions_;
  ObserverMap observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaScanManager);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_SCAN_MANAGER_H_
