// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_ENTRY_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_ENTRY_H_

#include <string>

namespace gdata {

// This is used as a bitmask for the cache state.
enum GDataCacheState {
  CACHE_STATE_NONE       = 0x0,
  CACHE_STATE_PINNED     = 0x1 << 0,
  CACHE_STATE_PRESENT    = 0x1 << 1,
  CACHE_STATE_DIRTY      = 0x1 << 2,
  CACHE_STATE_MOUNTED    = 0x1 << 3,
  CACHE_STATE_PERSISTENT = 0x1 << 4,
};

// Structure to store information of an existing cache file.
// Cache files are stored in 'tmp' or 'persistent' directory.
class GDataCacheEntry {
 public:
  GDataCacheEntry() : cache_state_(CACHE_STATE_NONE) {}

  GDataCacheEntry(const std::string& md5,
                  int cache_state)
      : md5_(md5),
        cache_state_(cache_state) {
  }

  // The MD5 of the cache file. This can be "local" if the file is
  // locally modified.
  const std::string& md5() const { return md5_; }

  // The cache state represented as a bitmask of GDataCacheState.
  int cache_state() const { return cache_state_; }

  void set_md5(const std::string& md5) { md5_ = md5; }
  void set_cache_state(int cache_state) { cache_state_ = cache_state; }

  // Returns true if the file is present locally.
  bool IsPresent() const { return cache_state_ & CACHE_STATE_PRESENT; }

  // Returns true if the file is pinned (i.e. available offline).
  bool IsPinned() const { return cache_state_ & CACHE_STATE_PINNED; }

  // Returns true if the file is dirty (i.e. modified locally).
  bool IsDirty() const { return cache_state_ & CACHE_STATE_DIRTY; }

  // Returns true if the file is a mounted archive file.
  bool IsMounted() const { return cache_state_ & CACHE_STATE_MOUNTED; }

  // Returns true if the file is in the persistent directory.
  bool IsPersistent() const { return cache_state_ & CACHE_STATE_PERSISTENT; }

  // Setters for the states describe above.
  void SetPresent(bool value) {
    if (value)
      cache_state_ |= CACHE_STATE_PRESENT;
    else
      cache_state_ &= ~CACHE_STATE_PRESENT;
  }
  void SetPinned(bool value) {
    if (value)
      cache_state_ |= CACHE_STATE_PINNED;
    else
      cache_state_ &= ~CACHE_STATE_PINNED;
  }
  void SetDirty(bool value) {
    if (value)
      cache_state_ |= CACHE_STATE_DIRTY;
    else
      cache_state_ &= ~CACHE_STATE_DIRTY;
  }
  void SetMounted(bool value) {
    if (value)
      cache_state_ |= CACHE_STATE_MOUNTED;
    else
      cache_state_ &= ~CACHE_STATE_MOUNTED;
  }
  void SetPersistent(bool value) {
    if (value)
      cache_state_ |= CACHE_STATE_PERSISTENT;
    else
      cache_state_ &= ~CACHE_STATE_PERSISTENT;
  }

  // For debugging purposes.
  std::string ToString() const;

 private:
  std::string md5_;
  int cache_state_;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_ENTRY_H_
