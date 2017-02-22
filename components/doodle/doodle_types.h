// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOODLE_DOODLE_TYPES_H_
#define COMPONENTS_DOODLE_DOODLE_TYPES_H_

#include "url/gurl.h"

namespace doodle {

enum class DoodleState {
  AVAILABLE,
  NO_DOODLE,
  DOWNLOAD_ERROR,
  PARSING_ERROR,
};

enum class DoodleType {
  UNKNOWN,
  SIMPLE,
  RANDOM,
  VIDEO,
  INTERACTIVE,
  INLINE_INTERACTIVE,
  SLIDESHOW,
};

// Information about a Doodle image. If the image is invalid, the |url| will be
// empty and invalid. By default the dimensions are 0.
struct DoodleImage {
  DoodleImage();
  ~DoodleImage();

  GURL url;
  int height;
  int width;
  bool is_animated_gif;
  bool is_cta;

  // Copying and assignment allowed.
};

// All information about a current doodle that can be fetched from the remote
// end. By default, all URLs are empty and therefore invalid.
struct DoodleConfig {
  DoodleConfig();
  DoodleConfig(const DoodleConfig& config);  // = default;
  ~DoodleConfig();

  DoodleType doodle_type;
  std::string alt_text;
  std::string interactive_html;

  base::Time expiry_date;
  GURL search_url;
  GURL target_url;
  GURL fullpage_interactive_url;

  DoodleImage large_image;
  DoodleImage large_cta_image;
  DoodleImage transparent_large_image;
};

}  // namespace doodle

#endif  // COMPONENTS_DOODLE_DOODLE_TYPES_H_
