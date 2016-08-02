// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/chrome_data_use_group_provider.h"

#include <stdint.h>

#include "chrome/browser/net/spdyproxy/chrome_data_use_group.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"

namespace net {
class URLRequest;
}  // namespace net

ChromeDataUseGroupProvider::ChromeDataUseGroupProvider()
    : // Size of the map is somewhat arbitrary. A larger size reduces the
      // chance of not finding an existing instance of |DataUseGroup| at the
      // cost of an increased memory footprint.
      render_frame_id_data_use_group_map(100) {}

ChromeDataUseGroupProvider::~ChromeDataUseGroupProvider() {}

scoped_refptr<data_reduction_proxy::DataUseGroup>
ChromeDataUseGroupProvider::GetDataUseGroup(const net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  int render_process_id, render_frame_id;
  if (!content::ResourceRequestInfo::GetRenderFrameForRequest(
      request, &render_process_id, &render_frame_id)) {
    return make_scoped_refptr(new ChromeDataUseGroup(request));
  }

  RenderFrameHostID key(render_process_id, render_frame_id);
  auto it = render_frame_id_data_use_group_map.Get(key);

  if (it != render_frame_id_data_use_group_map.end()) {
    return it->second;
  }

  scoped_refptr<data_reduction_proxy::DataUseGroup> source =
      new ChromeDataUseGroup(request);
  render_frame_id_data_use_group_map.Put(key, source);

  return source;
}
