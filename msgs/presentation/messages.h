// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MSGS_PRESENTATION_MESSAGES_H_
#define MSGS_PRESENTATION_MESSAGES_H_

#include <cstdint>
#include <string>
#include <vector>

namespace openscreen {
namespace msgs {
namespace presentation {

enum class Tag {
  kUrlAvailabilityRequest = 2000,
};

enum class UrlAvailability {
  kCompatible = 0,
  kNotCompatible = 1,
  kNotValid = 10,
};

struct UrlAvailabilityRequest {
  uint64_t request_id;
  std::vector<std::string> urls;
};

// TODO(btolsch): ErrorOr<> would remove the need for ssize_t here.
ssize_t EncodeUrlAvailabilityRequest(const UrlAvailabilityRequest& request,
                                     uint8_t* buffer,
                                     size_t length);
ssize_t DecodeUrlAvailabilityRequest(uint8_t* buffer,
                                     size_t length,
                                     UrlAvailabilityRequest* request);

}  // namespace presentation
}  // namespace msgs
}  // namespace openscreen

#endif  // MSGS_PRESENTATION_MESSAGES_H_
