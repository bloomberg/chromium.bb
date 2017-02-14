// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_REQUEST_METADATA_H_
#define COMPONENTS_IMAGE_FETCHER_REQUEST_METADATA_H_

#include <string>

namespace image_fetcher {

// Metadata for a URL request.
struct RequestMetadata {
  std::string mime_type;
  // HTTP response code.
  int response_code;
};

bool operator==(const RequestMetadata& lhs, const RequestMetadata& rhs);
bool operator!=(const RequestMetadata& lhs, const RequestMetadata& rhs);

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_REQUEST_METADATA_H_
