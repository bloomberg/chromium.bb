// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_UTIL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_UTIL_H_

#include "base/optional.h"
#include "third_party/webrtc/api/optional.h"

namespace content {

template <typename T>
base::Optional<T> ToBaseOptional(const absl::optional<T>& optional) {
  return optional ? base::Optional<T>(*optional) : base::nullopt;
}

template <typename T>
bool operator==(const base::Optional<T>& base_optional,
                const absl::optional<T>& absl_optional) {
  if (!base_optional)
    return !absl_optional;
  if (!absl_optional)
    return false;
  return *base_optional == *absl_optional;
}

template <typename T>
bool operator==(const absl::optional<T>& absl_optional,
                const base::Optional<T>& base_optional) {
  return base_optional == absl_optional;
}

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_UTIL_H_
