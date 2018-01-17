// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SERVICE_WORKER_MODES_H_
#define CONTENT_PUBLIC_COMMON_SERVICE_WORKER_MODES_H_

namespace content {

// Indicates no service worker provider.
static const int kInvalidServiceWorkerProviderId = -1;

// Whether this is a regular fetch, or a foreign fetch request (now removed).
// Duplicate of blink::mojom::ServiceWorkerFetchType.
// TODO(falken): Remove this since it's always FETCH.
enum class ServiceWorkerFetchType { FETCH, LAST = FETCH };

// Indicates whether service workers will receive fetch events for this request.
// TODO(falken): This enum made more sense when there was a foreign fetch mode.
// Find better names or fold this into a boolean.
enum class ServiceWorkerMode {
  // The relevant service worker, if any, will get a fetch event for this
  // request.
  ALL,
  // No service worker will get events for this request.
  NONE,
  LAST = NONE
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SERVICE_WORKER_MODES_H_
