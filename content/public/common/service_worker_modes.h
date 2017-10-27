// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SERVICE_WORKER_MODES_H_
#define CONTENT_PUBLIC_COMMON_SERVICE_WORKER_MODES_H_

namespace content {

// Indicates no service worker provider.
static const int kInvalidServiceWorkerProviderId = -1;

// The enum entries below are written to histograms and thus cannot be deleted
// or reordered.
// New entries must be added immediately before the end.
enum class FetchRedirectMode {
  FOLLOW_MODE,
  ERROR_MODE,
  MANUAL_MODE,
  LAST = MANUAL_MODE
};

// Whether this is a regular fetch, or a foreign fetch request.
// Duplicate of blink::mojom::ServiceWorkerFetchType.
enum class ServiceWorkerFetchType {
  FETCH,
  FOREIGN_FETCH,
  LAST = FOREIGN_FETCH
};

// Indicates which service workers will receive fetch events for this request.
enum class ServiceWorkerMode {
  // Relevant local and foreign service workers will get a fetch or
  // foreignfetch event for this request.
  ALL,
  // Only relevant foreign service workers will get a foreignfetch event for
  // this request.
  FOREIGN,
  // Neither local nor foreign service workers will get events for this
  // request.
  NONE,
  LAST = NONE
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SERVICE_WORKER_MODES_H_
