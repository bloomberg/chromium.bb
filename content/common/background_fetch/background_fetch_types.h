// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_BACKGROUND_FETCH_BACKGROUND_FETCH_TYPES_H_
#define CONTENT_COMMON_BACKGROUND_FETCH_BACKGROUND_FETCH_TYPES_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace content {

// Represents the optional options a developer can provide when starting a new
// Background Fetch fetch. Analogous to the following structure in the spec:
// https://wicg.github.io/background-fetch/#background-fetch-manager
struct CONTENT_EXPORT BackgroundFetchOptions {
  BackgroundFetchOptions();
  BackgroundFetchOptions(const BackgroundFetchOptions& other);
  ~BackgroundFetchOptions();

  std::vector<blink::Manifest::ImageResource> icons;
  std::string title;
  uint64_t download_total = 0;
};

// Represents the information associated with a Background Fetch registration.
// Analogous to the following structure in the spec:
// https://wicg.github.io/background-fetch/#background-fetch-registration
struct CONTENT_EXPORT BackgroundFetchRegistration {
  BackgroundFetchRegistration();
  BackgroundFetchRegistration(const BackgroundFetchRegistration& other);
  ~BackgroundFetchRegistration();

  // Corresponds to IDL 'id' attribute. Not unique - an active registration can
  // have the same |developer_id| as one or more inactive registrations.
  std::string developer_id;
  // Globally unique ID for the registration, generated in content/. Used to
  // distinguish registrations in case a developer re-uses |developer_id|s. Not
  // exposed to JavaScript.
  std::string unique_id;

  uint64_t upload_total = 0;
  uint64_t uploaded = 0;
  uint64_t download_total = 0;
  uint64_t downloaded = 0;
  // TODO(crbug.com/699957): Support the `activeFetches` member.
};

// Represents a request/response pair for a settled Background Fetch fetch.
// Analogous to the following structure in the spec:
// http://wicg.github.io/background-fetch/#backgroundfetchsettledfetch
struct CONTENT_EXPORT BackgroundFetchSettledFetch {
  BackgroundFetchSettledFetch();
  BackgroundFetchSettledFetch(const BackgroundFetchSettledFetch& other);
  ~BackgroundFetchSettledFetch();

  ServiceWorkerFetchRequest request;
  ServiceWorkerResponse response;
};

}  // namespace content

#endif  // CONTENT_COMMON_BACKGROUND_FETCH_BACKGROUND_FETCH_TYPES_H_
