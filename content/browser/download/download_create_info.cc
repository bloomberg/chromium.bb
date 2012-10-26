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
    int64 received_bytes,
    int64 total_bytes,
    int32 state,
    const net::BoundNetLog& bound_net_log,
    bool has_user_gesture,
    PageTransition transition_type)
    : start_time(start_time),
      received_bytes(received_bytes),
      total_bytes(total_bytes),
      state(state),
      download_id(DownloadId::Invalid()),
      has_user_gesture(has_user_gesture),
      transition_type(transition_type),
      db_handle(0),
      prompt_user_for_save_location(false),
      save_info(new DownloadSaveInfo()),
      request_bound_net_log(bound_net_log) {
}

DownloadCreateInfo::DownloadCreateInfo()
    : received_bytes(0),
      total_bytes(0),
      state(-1),
      download_id(DownloadId::Invalid()),
      has_user_gesture(false),
      transition_type(PAGE_TRANSITION_LINK),
      db_handle(0),
      prompt_user_for_save_location(false),
      save_info(new DownloadSaveInfo()) {
}

DownloadCreateInfo::~DownloadCreateInfo() {
}

std::string DownloadCreateInfo::DebugString() const {
  return base::StringPrintf("{"
                            " download_id = %s"
                            " url = \"%s\""
                            " received_bytes = %" PRId64
                            " total_bytes = %" PRId64
                            " prompt_user_for_save_location = %c"
                            " }",
                            download_id.DebugString().c_str(),
                            url().spec().c_str(),
                            received_bytes,
                            total_bytes,
                            prompt_user_for_save_location ? 'T' : 'F');
}

const GURL& DownloadCreateInfo::url() const {
  return url_chain.empty() ? GURL::EmptyGURL() : url_chain.back();
}

}  // namespace content
