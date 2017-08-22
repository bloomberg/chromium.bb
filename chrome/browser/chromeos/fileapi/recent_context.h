// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "url/gurl.h"

namespace chromeos {

// Holds parameters necessary for computation of recently modified file list.
// It is passed from RecentModel to RecentSource.
class RecentContext {
 public:
  // Constructs an invalid object.
  RecentContext();

  // Constructs a valid object.
  RecentContext(storage::FileSystemContext* file_system_context,
                const GURL& origin,
                size_t max_files,
                const base::Time& cutoff_time);

  RecentContext(const RecentContext& other);
  RecentContext(RecentContext&& other);
  ~RecentContext();
  RecentContext& operator=(const RecentContext& other);

  // Returns if this object is valid, otherwise false.
  bool is_valid() const { return is_valid_; }

  // FileSystemContext that can be used for file system operations.
  storage::FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

  // Origin of external file system URLs.
  // E.g. "chrome-extension://<extension-ID>/"
  const GURL& origin() const { return origin_; }

  // Maximum number of files a RecentSource is expected to return. It is fine to
  // return more files than requested here, but excessive items will be filtered
  // out by RecentModel.
  size_t max_files() const { return max_files_; }

  // Cut-off last modified time. RecentSource is expected to return files
  // modified at this time or later. It is fine to return older files than
  // requested here, but they will be filtered out by RecentModel.
  const base::Time& cutoff_time() const { return cutoff_time_; }

 private:
  bool is_valid_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  GURL origin_;
  size_t max_files_;
  base::Time cutoff_time_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_CONTEXT_H_
