// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_REQUEST_METADATA_H_
#define COMPONENTS_IMAGE_FETCHER_REQUEST_METADATA_H_

#include <string>

namespace image_fetcher {

struct RequestMetadata {
  std::string mime_type;
};

bool operator==(const RequestMetadata& lhs, const RequestMetadata& rhs);
bool operator!=(const RequestMetadata& lhs, const RequestMetadata& rhs);

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_REQUEST_METADATA_H_
