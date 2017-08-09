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

namespace content {

// Represents the definition of an icon developers can optionally provide with a
// Background Fetch fetch. Analogous to the following structure in the spec:
// https://wicg.github.io/background-fetch/#background-fetch-manager
//
// Parsing of the icon definitions as well as fetching an appropriate icon will
// be done by Blink in the renderer process. The browser process is expected to
// treat these values as opaque strings.
struct CONTENT_EXPORT IconDefinition {
  IconDefinition();
  IconDefinition(const IconDefinition& other);
  ~IconDefinition();

  std::string src;
  std::string sizes;
  std::string type;
};

// Represents the optional options a developer can provide when starting a new
// Background Fetch fetch. Analogous to the following structure in the spec:
// https://wicg.github.io/background-fetch/#background-fetch-manager
struct CONTENT_EXPORT BackgroundFetchOptions {
  BackgroundFetchOptions();
  BackgroundFetchOptions(const BackgroundFetchOptions& other);
  ~BackgroundFetchOptions();

  std::vector<IconDefinition> icons;
  std::string title;
  int64_t total_download_size = 0;
};

// Represents the information associated with a Background Fetch registration.
// Analogous to the following structure in the spec:
// https://wicg.github.io/background-fetch/#background-fetch-registration
struct CONTENT_EXPORT BackgroundFetchRegistration {
  BackgroundFetchRegistration();
  BackgroundFetchRegistration(const BackgroundFetchRegistration& other);
  ~BackgroundFetchRegistration();

  std::string id;
  std::vector<IconDefinition> icons;
  std::string title;
  int64_t total_download_size = 0;

  // TODO(peter): Support the `activeFetches` member of the specification.
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
