// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_FACTORY_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_FACTORY_H_

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/download/url_download_handler.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class DownloadDestinationObserver;
class DownloadFile;
struct DownloadSaveInfo;

class CONTENT_EXPORT DownloadFileFactory {
 public:
  virtual ~DownloadFileFactory();

  virtual DownloadFile* CreateFile(
      std::unique_ptr<DownloadSaveInfo> save_info,
      const base::FilePath& default_downloads_directory,
      std::unique_ptr<DownloadManager::InputStream> stream,
      uint32_t download_id,
      base::WeakPtr<DownloadDestinationObserver> observer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_FACTORY_H_
