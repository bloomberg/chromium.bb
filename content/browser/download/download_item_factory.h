// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadItemFactory is used to produce different DownloadItems.
// It is separate from the DownloadManager to allow download manager
// unit tests to control the items produced.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_
#pragma once

#include "content/browser/download/download_item_impl.h"

#include <string>

struct DownloadCreateInfo;
class DownloadRequestHandleInterface;
class FilePath;
class GURL;

namespace content {
class DownloadId;
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

  virtual content::DownloadItem* CreatePersistedItem(
      DownloadItemImpl::Delegate* delegate,
      content::DownloadId download_id,
      const content::DownloadPersistentStoreInfo& info,
      const net::BoundNetLog& bound_net_log) = 0;

  virtual content::DownloadItem* CreateActiveItem(
      DownloadItemImpl::Delegate* delegate,
      const DownloadCreateInfo& info,
      DownloadRequestHandleInterface* request_handle,
      bool is_otr,
      const net::BoundNetLog& bound_net_log) = 0;

  virtual content::DownloadItem* CreateSavePageItem(
      DownloadItemImpl::Delegate* delegate,
      const FilePath& path,
      const GURL& url,
      bool is_otr,
      content::DownloadId download_id,
      const std::string& mime_type,
      const net::BoundNetLog& bound_net_log) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_
