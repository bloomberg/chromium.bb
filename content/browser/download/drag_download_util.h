// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_UTIL_H_
#define CONTENT_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_UTIL_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "content/browser/download/drag_download_file.h"
#include "content/common/content_export.h"
#include "ui/base/dragdrop/download_file_interface.h"

class FilePath;
class GURL;
namespace net {
class FileStream;
}

namespace drag_download_util {

// Parse the download metadata set in DataTransfer.setData. The metadata
// consists of a set of the following values separated by ":"
// * MIME type
// * File name
// * URL
// If the file name contains special characters, they need to be escaped
// appropriately.
// For example, we can have
//   text/plain:example.txt:http://example.com/example.txt
CONTENT_EXPORT bool ParseDownloadMetadata(const string16& metadata,
                                          string16* mime_type,
                                          FilePath* file_name,
                                          GURL* url);

// Create a new file at the specified path. If the file already exists, try to
// insert the sequential unifier to produce a new file, like foo-01.txt.
// Return a FileStream if successful.
CONTENT_EXPORT net::FileStream* CreateFileStreamForDrop(FilePath* file_path);

// Implementation of DownloadFileObserver to finalize the download process.
class CONTENT_EXPORT PromiseFileFinalizer : public ui::DownloadFileObserver {
 public:
  explicit PromiseFileFinalizer(DragDownloadFile* drag_file_downloader);
  virtual ~PromiseFileFinalizer();

  // DownloadFileObserver methods.
  virtual void OnDownloadCompleted(const FilePath& file_path) OVERRIDE;
  virtual void OnDownloadAborted() OVERRIDE;

 private:
  void Cleanup();

  scoped_refptr<DragDownloadFile> drag_file_downloader_;

  DISALLOW_COPY_AND_ASSIGN(PromiseFileFinalizer);
};

}  // namespace drag_download_util

#endif  // CONTENT_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_UTIL_H_
