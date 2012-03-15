// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
#pragma once

#include <vector>

#include "base/callback.h"
#include "base/string16.h"
#include "content/common/content_export.h"

class FilePath;

namespace base {
class Time;
}

namespace content {

class BrowserContext;

// Represents the per-BrowserContext Local Storage data.
class DOMStorageContext {
 public:
  typedef base::Callback<void(const std::vector<FilePath>&)>
      GetAllStorageFilesCallback;

  // Returns all the file paths of local storage files to the given callback.
  virtual void GetAllStorageFiles(
      const GetAllStorageFilesCallback& callback) = 0;

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

 protected:
  virtual ~DOMStorageContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
