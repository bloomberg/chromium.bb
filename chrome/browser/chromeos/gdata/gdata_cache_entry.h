// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_ENTRY_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_ENTRY_H_

#include <string>

namespace gdata {

// Structure to store information of an existing cache file.
// Cache files are stored in 'tmp' or 'persistent' directory.
class GDataCacheEntry {
 public:
  GDataCacheEntry()
      : is_present_(false),
        is_pinned_(false),
        is_dirty_(false),
        is_mounted_(false),
        is_persistent_(false) {
  }

  // The MD5 of the cache file. This can be "local" if the file is
  // locally modified.
  const std::string& md5() const { return md5_; }
  void set_md5(const std::string& md5) { md5_ = md5; }

  // Returns true if the file is present locally.
  bool is_present() const { return is_present_; }

  // Returns true if the file is pinned (i.e. available offline).
  bool is_pinned() const { return is_pinned_; }

  // Returns true if the file is dirty (i.e. modified locally).
  bool is_dirty() const { return is_dirty_; }

  // Returns true if the file is a mounted archive file.
  bool is_mounted() const { return is_mounted_; }

  // Returns true if the file is in the persistent directory.
  bool is_persistent() const { return is_persistent_; }

  // Setters for the states describe above.
  void set_is_present(bool value) { is_present_ = value; }
  void set_is_pinned(bool value) { is_pinned_ = value; }
  void set_is_dirty(bool value) { is_dirty_ = value; }
  void set_is_mounted(bool value) { is_mounted_ = value; }
  void set_is_persistent(bool value) { is_persistent_ = value; }

 private:
  std::string md5_;
  bool is_present_;
  bool is_pinned_;
  bool is_dirty_;
  bool is_mounted_;
  bool is_persistent_;
  // When adding a new state, be sure to update TestGDataCacheState and test
  // functions defined in gdata_test_util.cc.
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_ENTRY_H_
