// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_types.h"

namespace doodle {

bool DoodleImage::operator==(const DoodleImage& other) const {
  return url == other.url && height == other.height && width == other.width &&
         is_animated_gif == other.is_animated_gif && is_cta == other.is_cta;
}

bool DoodleImage::operator!=(const DoodleImage& other) const {
  return !(*this == other);
}

bool DoodleConfig::IsEquivalent(const DoodleConfig& other) const {
  // Note: This compares all fields except for |time_to_live|, which by
  // definition isn't constant over time, and shouldn't be in DoodleConfig in
  // the first place.
  return doodle_type == other.doodle_type && alt_text == other.alt_text &&
         interactive_html == other.interactive_html &&
         search_url == other.search_url && target_url == other.target_url &&
         fullpage_interactive_url == other.fullpage_interactive_url &&
         large_image == other.large_image &&
         large_cta_image == other.large_cta_image &&
         transparent_large_image == other.transparent_large_image;
}

}  // namespace doodle
