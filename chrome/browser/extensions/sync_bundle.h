// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYNC_BUNDLE_H_
#define CHROME_BROWSER_EXTENSIONS_SYNC_BUNDLE_H_

namespace extensions {

class Extension;

// Common interface between AppSyncBundle and ExtensionSyncBundle.
class SyncBundle {
 public:
  virtual ~SyncBundle() {}

  // Has this bundle started syncing yet?
  virtual bool IsSyncing() const = 0;

  // Syncs changes to |extension|.
  virtual void SyncChangeIfNeeded(const Extension& extension) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYNC_BUNDLE_H_
