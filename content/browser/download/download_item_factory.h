// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadItemFactory is used to produce different DownloadItems.
// It is separate from the DownloadManager to allow download manager
// unit tests to control the items produced.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_item.h"

class FilePath;
class GURL;

namespace net {
class BoundNetLog;
}

namespace content {

class DownloadItem;
class DownloadItemImpl;
class DownloadItemImplDelegate;
class DownloadRequestHandleInterface;
struct DownloadCreateInfo;

class DownloadItemFactory {
public:
  virtual ~DownloadItemFactory() {}

  virtual DownloadItemImpl* CreatePersistedItem(
      DownloadItemImplDelegate* delegate,
      DownloadId download_id,
      const FilePath& path,
      const GURL& url,
      const GURL& referrer_url,
      const base::Time& start_time,
      const base::Time& end_time,
      int64 received_bytes,
      int64 total_bytes,
      content::DownloadItem::DownloadState state,
      bool opened,
      const net::BoundNetLog& bound_net_log) = 0;

  virtual DownloadItemImpl* CreateActiveItem(
      DownloadItemImplDelegate* delegate,
      const DownloadCreateInfo& info,
      const net::BoundNetLog& bound_net_log) = 0;

  virtual DownloadItemImpl* CreateSavePageItem(
      DownloadItemImplDelegate* delegate,
      const FilePath& path,
      const GURL& url,
      DownloadId download_id,
      const std::string& mime_type,
      const net::BoundNetLog& bound_net_log) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_
