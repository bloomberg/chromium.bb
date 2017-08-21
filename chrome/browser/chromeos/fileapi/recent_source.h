// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "storage/browser/fileapi/file_system_url.h"

namespace chromeos {

class RecentContext;
class RecentFile;

// Interface class for a source of recent files.
//
// Recent file system retrieves recent files from several sources such as
// local directories and cloud storages. To provide files to Recent file
// system, this interface should be implemented for each source.
//
// Note: If a source implementation depends on KeyedServices, remember to add
// dependencies in RecentModelFactory.
//
// All member functions must be called on the UI thread.
class RecentSource {
 public:
  using GetRecentFilesCallback =
      base::OnceCallback<void(std::vector<RecentFile> files)>;

  virtual ~RecentSource();

  // Retrieves a list of recent files from this source.
  //
  // You can assume that, once this function is called, it is not called again
  // until the callback is invoked. This means that you can safely save internal
  // states to compute recent files in member variables.
  virtual void GetRecentFiles(RecentContext context,
                              GetRecentFilesCallback callback) = 0;

 protected:
  RecentSource();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_SOURCE_H_
