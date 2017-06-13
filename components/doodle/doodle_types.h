// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOODLE_DOODLE_TYPES_H_
#define COMPONENTS_DOODLE_DOODLE_TYPES_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

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

// Information about a Doodle image.
struct DoodleImage {
  DoodleImage(const GURL& url);
  ~DoodleImage();

  static base::Optional<DoodleImage> FromDictionary(
      const base::DictionaryValue& dict,
      const base::Optional<GURL>& base_url);

  std::unique_ptr<base::DictionaryValue> ToDictionary() const;

  bool operator==(const DoodleImage& other) const;
  bool operator!=(const DoodleImage& other) const;

  GURL url;

  // Copying and assignment allowed.
};

// All information about a current doodle that can be fetched from the remote
// end. By default, all URLs are empty and therefore invalid.
struct DoodleConfig {
  DoodleConfig(DoodleType doodle_type, const DoodleImage& large_image);
  DoodleConfig(const DoodleConfig& config);  // = default;
  ~DoodleConfig();

  static base::Optional<DoodleConfig> FromDictionary(
      const base::DictionaryValue& dict,
      const base::Optional<GURL>& base_url);

  std::unique_ptr<base::DictionaryValue> ToDictionary() const;

  bool operator==(const DoodleConfig& other) const;
  bool operator!=(const DoodleConfig& other) const;

  DoodleType doodle_type;
  std::string alt_text;

  GURL target_url;

  DoodleImage large_image;
  base::Optional<DoodleImage> large_cta_image;

  // Copying and assignment allowed.
};

}  // namespace doodle

#endif  // COMPONENTS_DOODLE_DOODLE_TYPES_H_
