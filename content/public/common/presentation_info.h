// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PRESENTATION_INFO_H_
#define CONTENT_PUBLIC_COMMON_PRESENTATION_INFO_H_

#include <string>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// Represents a presentation that has been established via either
// browser actions or Presentation API.
struct CONTENT_EXPORT PresentationInfo {
  PresentationInfo() = default;
  PresentationInfo(const GURL& presentation_url,
                   const std::string& presentation_id);
  ~PresentationInfo();

  static constexpr size_t kMaxIdLength = 256;

  GURL presentation_url;
  std::string presentation_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PRESENTATION_INFO_H_
