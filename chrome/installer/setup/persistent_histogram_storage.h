// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_PERSISTENT_HISTOGRAM_STORAGE_H_
#define CHROME_INSTALLER_SETUP_PERSISTENT_HISTOGRAM_STORAGE_H_

#include "base/files/file_path.h"
#include "base/macros.h"

namespace installer {

// When a PersistentHistogramStorage is destructed, histograms recorded during
// its lifetime are persisted in the directory set by set_storage_dir().
// Histograms are not persisted if set_storage_dir() has not been called or if
// the storage directory does not exist on destruction.
// PersistentHistogramStorage should be instantiated as early as possible in the
// process lifetime and should never be instantiated again. Persisted histograms
// will eventually be reported by the browser process.
class PersistentHistogramStorage {
 public:
  PersistentHistogramStorage();
  ~PersistentHistogramStorage();

  // Sets |storage_dir| as the directory in which histograms will be persisted
  // when this PersistentHistogramStorage is destructed.
  void set_storage_dir(const base::FilePath& storage_dir) {
    storage_dir_ = storage_dir;
  }

  // Returns a directory in which setup histograms must be persisted to be
  // reported by a product installed in |target_path|. Should be used as
  // argument to set_storage_dir().
  static base::FilePath GetReportedStorageDir(
      const base::FilePath& target_path);

 private:
  base::FilePath storage_dir_;

  DISALLOW_COPY_AND_ASSIGN(PersistentHistogramStorage);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_PERSISTENT_HISTOGRAM_STORAGE_H_
