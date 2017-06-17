// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_TYPES_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_TYPES_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/offline_pages/core/client_id.h"
#include "url/gurl.h"

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

// List of states a prefetch item can be at during its progress through the
// prefetching process. They follow somewhat the order below, but some states
// might be skipped.
enum class PrefetchItemState {
  // New request just received from the client.
  NEW_REQUEST,
  // The item has been included in a GeneratePageBundle RPC requesting the
  // creation of an archive for its URL.
  SENT_GENERATE_PAGE_BUNDLE,
  // The archive was not immediately available (cached) upon the request and
  // is now waiting for a GCM message notifying of its archiving operation
  // completion.
  AWAITING_GCM,
  // The GCM message notifying of the archiving operation completion was
  // received for this item.
  RECEIVED_GCM,
  // A GetOperation RPC was sent for this item to query for the final results
  // of its archiving request.
  SENT_GET_OPERATION,
  // Information was received about a successfully created archive for this
  // item that can now be downloaded.
  RECEIVED_BUNDLE,
  // This item's archive is currently being downloaded.
  DOWNLOADING,
  // Item has finished processing, successfully or otherwise, and is waiting to
  // be processed for stats reporting to UMA.
  FINISHED,
  // UMA stats have been reported and the item is being kept just long enough
  // to confirm that the same URL is not being repeatedly requested by its
  // client.
  ZOMBIE,
};

// Error codes used to identify the reason why a prefetch item has finished
// processing.
enum class PrefetchItemErrorCode {
  SUCCESS,
  EXPIRED,
};

// Callback invoked upon completion of a prefetch request.
using PrefetchRequestFinishedCallback =
    base::Callback<void(PrefetchRequestStatus status,
                        const std::string& operation_name,
                        const std::vector<RenderPageInfo>& pages)>;

// Holds information about a suggested URL to be prefetched.
struct PrefetchURL {
  PrefetchURL(const std::string& id, const GURL& url) : id(id), url(url) {}

  // Client provided ID to allow the matching of provided URLs to the respective
  // work item in the prefetching system within that client's assigned
  // namespace. It can be any string value and it will not be used internally
  // for de-duplication.
  std::string id;

  // This URL will be prefetched by the service.
  GURL url;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_TYPES_H_
