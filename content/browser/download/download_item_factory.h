// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadItemFactory is used to produce different download::DownloadItems.
// It is separate from the DownloadManager to allow download manager
// unit tests to control the items produced.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "components/download/public/common/download_item.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace download {
struct DownloadCreateInfo;
class DownloadItemImpl;
class DownloadItemImplDelegate;
class DownloadRequestHandleInterface;
}  // namespace download

namespace content {

class DownloadItemFactory {
public:
  virtual ~DownloadItemFactory() {}

  virtual download::DownloadItemImpl* CreatePersistedItem(
      download::DownloadItemImplDelegate* delegate,
      const std::string& guid,
      uint32_t download_id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_refererr_url,
      const std::string& mime_type,
      const std::string& original_mime_type,
      base::Time start_time,
      base::Time end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64_t received_bytes,
      int64_t total_bytes,
      const std::string& hash,
      download::DownloadItem::DownloadState state,
      download::DownloadDangerType danger_type,
      download::DownloadInterruptReason interrupt_reason,
      bool opened,
      base::Time last_access_time,
      bool transient,
      const std::vector<download::DownloadItem::ReceivedSlice>&
          received_slices) = 0;

  virtual download::DownloadItemImpl* CreateActiveItem(
      download::DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const download::DownloadCreateInfo& info) = 0;

  virtual download::DownloadItemImpl* CreateSavePageItem(
      download::DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const base::FilePath& path,
      const GURL& url,
      const std::string& mime_type,
      std::unique_ptr<download::DownloadRequestHandleInterface>
          request_handle) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_
