// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/request_metadata.h"

namespace image_fetcher {

bool operator==(const RequestMetadata& lhs, const RequestMetadata& rhs) {
  return lhs.mime_type == rhs.mime_type &&
         lhs.response_code == rhs.response_code;
}

bool operator!=(const RequestMetadata& lhs, const RequestMetadata& rhs) {
  return !(lhs == rhs);
}

}  // namespace image_fetcher
