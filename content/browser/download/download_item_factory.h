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

struct DownloadCreateInfo;

class DownloadItemImpl;
class DownloadItemImplDelegate;
class DownloadRequestHandleInterface;
class FilePath;
class GURL;

namespace content {
class DownloadItem;
struct DownloadPersistentStoreInfo;
}

namespace net {
class BoundNetLog;
}

namespace content {

class DownloadItemFactory {
public:
  virtual ~DownloadItemFactory() {}

  virtual DownloadItemImpl* CreatePersistedItem(
      DownloadItemImplDelegate* delegate,
      content::DownloadId download_id,
      const content::DownloadPersistentStoreInfo& info,
      const net::BoundNetLog& bound_net_log) = 0;

  virtual DownloadItemImpl* CreateActiveItem(
      DownloadItemImplDelegate* delegate,
      const DownloadCreateInfo& info,
      scoped_ptr<DownloadRequestHandleInterface> request_handle,
      bool is_otr,
      const net::BoundNetLog& bound_net_log) = 0;

  virtual DownloadItemImpl* CreateSavePageItem(
      DownloadItemImplDelegate* delegate,
      const FilePath& path,
      const GURL& url,
      bool is_otr,
      content::DownloadId download_id,
      const std::string& mime_type,
      const net::BoundNetLog& bound_net_log) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_
