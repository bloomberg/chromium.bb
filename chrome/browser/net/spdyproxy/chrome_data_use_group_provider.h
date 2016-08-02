// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_CHROME_DATA_USE_GROUP_PROVIDER_H_
#define CHROME_BROWSER_NET_SPDYPROXY_CHROME_DATA_USE_GROUP_PROVIDER_H_

#include <utility>

#include "base/containers/mru_cache.h"
#include "base/memory/ref_counted.h"
#include "components/data_reduction_proxy/core/browser/data_use_group.h"
#include "components/data_reduction_proxy/core/browser/data_use_group_provider.h"

namespace net {
class URLRequest;
}

class ChromeDataUseGroupProvider
    : public data_reduction_proxy::DataUseGroupProvider {
 public:
  // Constructor for Chrome implementation of |DataUseGroupProvider|.
  ChromeDataUseGroupProvider();

  // Destructor for Chrome implementation of |DataUseGroupProvider.
  ~ChromeDataUseGroupProvider();

  // Returns the |DataUseGroup| that tracks data use that is keyed on the given
  // URL request.
  // This method does not currently guarantee that a single |DataUseGroup|
  // instance is created for a page load. However, instances initiated from the
  // same page will return the same value from |getHostname()|, which is the
  // only functionality currently supported by this class. This is sufficient
  // for the current use case of ascribing data use to a hostname.
  // TODO(kundaji): Enforce that a unique instance of |DataUseGroup| is
  // created for each data source that will be made visible to the end user.
  scoped_refptr<data_reduction_proxy::DataUseGroup> GetDataUseGroup(
      const net::URLRequest* request) override;

 private:
  typedef std::pair<int, int> RenderFrameHostID;

  // Map of <|render_process_id|, |render_frame_id|> to |DataUseGroup| on the
  // IO thread. This map does not contain all active |DataUseGroup|
  // instances, but only the most recent ones. This map provides access to
  // |DataUseGroup| instances for frames that might have already been destroyed.
  base::MRUCache<RenderFrameHostID,
                 scoped_refptr<data_reduction_proxy::DataUseGroup>>
      render_frame_id_data_use_group_map;
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_CHROME_DATA_USE_GROUP_PROVIDER_H_
