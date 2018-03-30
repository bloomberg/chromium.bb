// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_PERSISTENT_HISTOGRAM_STORAGE_H_
#define BASE_METRICS_PERSISTENT_HISTOGRAM_STORAGE_H_

#include "base/base_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace base {

// This class creates a fixed sized persistent memory to allow histograms to be
// stored in it. When a PersistentHistogramStorage is destructed, histograms
// recorded during its lifetime are persisted in the directory
// |storage_base_dir_|/|allocator_name| (see the ctor for allocator_name).
// Histograms are not persisted if the storage directory does not exist on
// destruction. PersistentHistogramStorage should be instantiated as early as
// possible in the process lifetime and should never be instantiated again.
// Persisted histograms will eventually be reported by Chrome.
class BASE_EXPORT PersistentHistogramStorage {
 public:
  enum class StorageDirCreation { kEnable, kDisable };

  // Creates a process-wide storage location for histograms that will be written
  // to a file within a directory provided by |set_storage_base_dir()| on
  // destruction.
  // The |allocator_name| is used both as an internal name for the allocator,
  // well as the leaf directory name for the file to which the histograms are
  // persisted. The string must be ASCII.
  // |storage_dir_create_action| determines that if this instance is
  // responsible for creating the storage directory.
  PersistentHistogramStorage(StringPiece allocator_name,
                             StorageDirCreation storage_dir_create_action);

  ~PersistentHistogramStorage();

  // The storage directory isn't always known during initial construction so
  // it's set separately. The last one wins if there are multiple calls to this
  // method.
  void set_storage_base_dir(const FilePath& storage_base_dir) {
    storage_base_dir_ = storage_base_dir;
  }

 private:
  // Metrics files are written into directory
  // |storage_base_dir_|/|allocator_name| (see the ctor for allocator_name).
  FilePath storage_base_dir_;

  // A flag indicating if the class instance is responsible for creating the
  // storage directory.
  const StorageDirCreation storage_dir_create_action_;

  DISALLOW_COPY_AND_ASSIGN(PersistentHistogramStorage);
};

}  // namespace base

#endif  // BASE_METRICS_PERSISTENT_HISTOGRAM_STORAGE_H_
