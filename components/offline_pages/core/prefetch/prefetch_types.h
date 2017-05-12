// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_TYPES_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_TYPES_H_

#include "base/macros.h"
#include "base/time/time.h"

namespace offline_pages {

// Status for sending prefetch request to the server.
enum class PrefetchRequestStatus {
  // Request completed successfully.
  SUCCESS,
  // Request failed due to to local network problem, unrelated to server load
  // levels. The caller will simply reschedule the retry in the next available
  // WiFi window after 15 minutes have passed.
  SHOULD_RETRY_WITHOUT_BACKOFF,
  // Request failed probably related to transient server problems. The caller
  // will reschedule the retry with backoff included.
  SHOULD_RETRY_WITH_BACKOFF,
  // Request failed with error indicating that the server no longer knows how
  // to service a request. The caller will prevent network requests for the
  // period of 1 day.
  SHOULD_SUSPEND
};

// Status indicating the page rendering status in the server.
enum class RenderStatus {
  // The page is rendered.
  RENDERED,
  // The page is still being processed.
  PENDING,
  // The page failed to render.
  FAILED,
  // Failed due to bundle size limits.
  EXCEEDED_LIMIT
};

// Information about the page rendered in the server.
struct RenderPageInfo {
  RenderPageInfo();
  RenderPageInfo(const RenderPageInfo& other);

  // The URL of the page that was rendered.
  std::string url;
  // The final URL after redirects. Empty if the final URL is url.
  std::string redirect_url;
  // Status of the render attempt.
  RenderStatus status = RenderStatus::FAILED;
  // Resource name for the body which can be read via the ByteStream API.
  // Set only when |status| is RENDERED.
  std::string body_name;
  // Length of the body in bytes. Set only when |status| is RENDERED.
  int64_t body_length = 0LL;
  // Time the page was rendered. Set only when |status| is RENDERED.
  base::Time render_time;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_TYPES_H_
