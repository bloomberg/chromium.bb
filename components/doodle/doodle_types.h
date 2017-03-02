// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOODLE_DOODLE_TYPES_H_
#define COMPONENTS_DOODLE_DOODLE_TYPES_H_

#include "base/time/time.h"
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

  bool operator==(const DoodleImage& other) const;
  bool operator!=(const DoodleImage& other) const;

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

  // Checks whether this config is "equivalent" to the other. This compares all
  // fields for equality, except for |expiry_date|.
  bool IsEquivalent(const DoodleConfig& other) const;

  DoodleType doodle_type;
  std::string alt_text;
  std::string interactive_html;

  GURL search_url;
  GURL target_url;
  GURL fullpage_interactive_url;

  DoodleImage large_image;
  DoodleImage large_cta_image;
  DoodleImage transparent_large_image;

  // TODO(treib,fhorschig): Don't expose this? Clients don't care about it.
  base::TimeDelta time_to_live;

  // Copying and assignment allowed.
};

}  // namespace doodle

#endif  // COMPONENTS_DOODLE_DOODLE_TYPES_H_
