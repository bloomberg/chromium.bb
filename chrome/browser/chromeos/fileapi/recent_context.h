// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "url/gurl.h"

namespace chromeos {

// Holds several "context" objects to be used for operations in Recent file
// system.
class RecentContext {
 public:
  RecentContext();
  RecentContext(storage::FileSystemContext* file_system_context,
                const GURL& origin);
  RecentContext(const RecentContext& other);
  RecentContext(RecentContext&& other);
  ~RecentContext();
  RecentContext& operator=(const RecentContext& other);

  bool is_valid() const { return is_valid_; }

  storage::FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }
  const GURL& origin() const { return origin_; }

 private:
  bool is_valid_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  GURL origin_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_CONTEXT_H_
