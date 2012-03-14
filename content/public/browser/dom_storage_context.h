// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

class FilePath;

namespace base {
class SequencedTaskRunner;
class Time;
}

namespace content {

class BrowserContext;

// Represents the per-BrowserContext Local Storage data.
// Call these methods only on tasks scheduled via it's task_runner().
class DOMStorageContext : public base::RefCountedThreadSafe<DOMStorageContext> {
 public:
  virtual ~DOMStorageContext() {}

  // Returns a task runner which should be used to schedule tasks
  // which can invoke the other methods of this interface.
  virtual base::SequencedTaskRunner* task_runner() const = 0;

  // Returns all the file paths of local storage files.
  virtual std::vector<FilePath> GetAllStorageFiles() = 0;

  // Get the file name of the local storage file for the given origin.
  virtual FilePath GetFilePath(const string16& origin_id) const = 0;

  // Deletes the local storage file for the given origin.
  virtual void DeleteForOrigin(const string16& origin_id) = 0;

  // Deletes a single local storage file.
  virtual void DeleteLocalStorageFile(const FilePath& file_path) = 0;

  // Delete any local storage files that have been touched since the cutoff
  // date that's supplied. Protected origins, per the SpecialStoragePolicy,
  // are not deleted by this method.
  virtual void DeleteDataModifiedSince(const base::Time& cutoff) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
