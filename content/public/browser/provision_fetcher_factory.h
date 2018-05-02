// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PROVISION_FETCHER_FACTORY_H
#define CONTENT_PUBLIC_BROWSER_PROVISION_FETCHER_FACTORY_H

#include <memory>

#include "content/common/content_export.h"

namespace media {
class ProvisionFetcher;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

// Factory method for media::ProvisionFetcher objects.

CONTENT_EXPORT
std::unique_ptr<media::ProvisionFetcher> CreateProvisionFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PROVISION_FETCHER_FACTORY_H
