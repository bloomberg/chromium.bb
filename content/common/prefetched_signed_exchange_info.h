// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PREFETCHED_SIGNED_EXCHANGE_INFO_H_
#define CONTENT_COMMON_PREFETCHED_SIGNED_EXCHANGE_INFO_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/hash_value.h"
#include "services/network/public/cpp/resource_response.h"
#include "url/gurl.h"

namespace content {

// Used for SignedExchangeSubresourcePrefetch.
// This struct keeps the information about a prefetched signed exchange. It is
// used in CommitNavigationParams to be passed from the browser process to the
// renderer process in CommitNavigation IPC.
struct CONTENT_EXPORT PrefetchedSignedExchangeInfo {
  PrefetchedSignedExchangeInfo();
  PrefetchedSignedExchangeInfo(const PrefetchedSignedExchangeInfo&);
  PrefetchedSignedExchangeInfo(
      const GURL& outer_url,
      const net::SHA256HashValue& header_integrity,
      const GURL& inner_url,
      const network::ResourceResponseHead& inner_response,
      mojo::MessagePipeHandle loader_factory_handle);
  ~PrefetchedSignedExchangeInfo();

  GURL outer_url;
  net::SHA256HashValue header_integrity;
  GURL inner_url;
  network::ResourceResponseHead inner_response;
  mojo::MessagePipeHandle loader_factory_handle;
};

}  // namespace content

#endif  // CONTENT_COMMON_PREFETCHED_SIGNED_EXCHANGE_INFO_H_
