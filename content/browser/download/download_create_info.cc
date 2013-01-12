// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_create_info.h"

#include <string>

#include "base/format_macros.h"
#include "base/stringprintf.h"

namespace content {

DownloadCreateInfo::DownloadCreateInfo(
    const base::Time& start_time,
    int64 total_bytes,
    const net::BoundNetLog& bound_net_log,
    bool has_user_gesture,
    PageTransition transition_type)
    : start_time(start_time),
      total_bytes(total_bytes),
      download_id(DownloadId::Invalid()),
      has_user_gesture(has_user_gesture),
      transition_type(transition_type),
      save_info(new DownloadSaveInfo()),
      request_bound_net_log(bound_net_log) {
}

DownloadCreateInfo::DownloadCreateInfo()
    : total_bytes(0),
      download_id(DownloadId::Invalid()),
      has_user_gesture(false),
      transition_type(PAGE_TRANSITION_LINK),
      save_info(new DownloadSaveInfo()) {
}

DownloadCreateInfo::~DownloadCreateInfo() {
}

std::string DownloadCreateInfo::DebugString() const {
  return base::StringPrintf("{"
                            " download_id = %s"
                            " url = \"%s\""
                            " request_handle = %s"
                            " total_bytes = %" PRId64
                            " }",
                            download_id.DebugString().c_str(),
                            url().spec().c_str(),
                            request_handle.DebugString().c_str(),
                            total_bytes);
}

const GURL& DownloadCreateInfo::url() const {
  return url_chain.empty() ? GURL::EmptyGURL() : url_chain.back();
}

}  // namespace content
