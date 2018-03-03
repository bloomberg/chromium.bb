// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UTILS_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UTILS_H_

#include <memory>

#include "base/optional.h"
#include "content/common/content_export.h"

namespace download {
struct DownloadEntry;
class DownloadUrlParameters;
}  // namespace download

namespace net {
class URLRequest;
}  // namespace net

namespace network {
struct ResourceRequest;
}

namespace storage {
class BlobStorageContext;
}

namespace content {

class BrowserContext;
class ResourceContext;

// Create a ResourceRequest from |params|.
std::unique_ptr<network::ResourceRequest> CONTENT_EXPORT
CreateResourceRequest(download::DownloadUrlParameters* params);

// Create a URLRequest from |params|.
std::unique_ptr<net::URLRequest> CONTENT_EXPORT
CreateURLRequestOnIOThread(download::DownloadUrlParameters* params);

// Get the entry based on |guid| from in progress cache.
CONTENT_EXPORT base::Optional<download::DownloadEntry> GetInProgressEntry(
    const std::string& guid,
    BrowserContext* browser_context);

storage::BlobStorageContext* BlobStorageContextGetter(
    ResourceContext* resource_context);

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UTILS_H_
