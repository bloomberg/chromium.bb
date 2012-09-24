// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_FACTORY_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/browser/download/download_create_info.h"
#include "googleurl/src/gurl.h"

namespace net {
class BoundNetLog;
}

namespace content {

struct DownloadSaveInfo;

class ByteStreamReader;
class DownloadDestinationObserver;
class DownloadFile;
class DownloadManager;

class CONTENT_EXPORT DownloadFileFactory {
 public:
  virtual ~DownloadFileFactory();

  virtual content::DownloadFile* CreateFile(
      DownloadCreateInfo* info,
      scoped_ptr<content::ByteStreamReader> stream,
      DownloadManager* download_manager,
      bool calculate_hash,
      const net::BoundNetLog& bound_net_log);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_FACTORY_H_
