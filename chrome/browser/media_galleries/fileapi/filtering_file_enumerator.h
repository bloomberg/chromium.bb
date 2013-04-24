// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_FILTERING_FILE_ENUMERATOR_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_FILTERING_FILE_ENUMERATOR_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "webkit/fileapi/file_system_file_util.h"

namespace chrome {

// This class wraps another file enumerator and filters out non-media files
// from its result, refering given MediaPathFilter.
class FilteringFileEnumerator
    : public fileapi::FileSystemFileUtil::AbstractFileEnumerator {
 public:
  FilteringFileEnumerator(
      scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
          base_enumerator,
      MediaPathFilter* filter);
  virtual ~FilteringFileEnumerator();

  virtual base::FilePath Next() OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual base::Time LastModifiedTime() OVERRIDE;
  virtual bool IsDirectory() OVERRIDE;

 private:
  // The file enumerator to be wrapped.
  scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
      base_enumerator_;

  // Path filter to filter out non-Media files.
  MediaPathFilter* filter_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_FILTERING_FILE_ENUMERATOR_H_
