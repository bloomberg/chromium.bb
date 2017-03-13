// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_types.h"

#include "base/values.h"

namespace doodle {

namespace {

DoodleType DoodleTypeFromString(const std::string& type_str) {
  if (type_str == "SIMPLE") {
    return DoodleType::SIMPLE;
  }
  if (type_str == "RANDOM") {
    return DoodleType::RANDOM;
  }
  if (type_str == "VIDEO") {
    return DoodleType::VIDEO;
  }
  if (type_str == "INTERACTIVE") {
    return DoodleType::INTERACTIVE;
  }
  if (type_str == "INLINE_INTERACTIVE") {
    return DoodleType::INLINE_INTERACTIVE;
  }
  if (type_str == "SLIDESHOW") {
    return DoodleType::SLIDESHOW;
  }
  return DoodleType::UNKNOWN;
}

GURL ResolvePossiblyRelativeUrl(const std::string& url_str,
                                const base::Optional<GURL>& base_url) {
  if (!base_url.has_value()) {
    return GURL(url_str);
  }
  return base_url->Resolve(url_str);
}

GURL ParseUrl(const base::DictionaryValue& parent_dict,
              const std::string& key,
              const base::Optional<GURL>& base_url) {
  std::string url_str;
  if (!parent_dict.GetString(key, &url_str) || url_str.empty()) {
    return GURL();
  }
  return ResolvePossiblyRelativeUrl(url_str, base_url);
}

base::Optional<DoodleImage> ParseImage(const base::DictionaryValue& parent_dict,
                                       const std::string& key,
                                       const base::Optional<GURL>& base_url) {
  const base::DictionaryValue* image_dict = nullptr;
  if (!parent_dict.GetDictionary(key, &image_dict)) {
    return base::nullopt;
  }
  return DoodleImage::FromDictionary(*image_dict, base_url);
}

}  // namespace

// static
base::Optional<DoodleImage> DoodleImage::FromDictionary(
    const base::DictionaryValue& dict,
    const base::Optional<GURL>& base_url) {
  DoodleImage image;

  // The URL is the only required field.
  image.url = ParseUrl(dict, "url", base_url);
  if (!image.url.is_valid()) {
    return base::nullopt;
  }

  dict.GetInteger("height", &image.height);
  dict.GetInteger("width", &image.width);
  dict.GetBoolean("is_animated_gif", &image.is_animated_gif);
  dict.GetBoolean("is_cta", &image.is_cta);

  return image;
}

DoodleImage::DoodleImage()
    : height(0), width(0), is_animated_gif(false), is_cta(false) {}
DoodleImage::~DoodleImage() = default;

bool DoodleImage::operator==(const DoodleImage& other) const {
  return url == other.url && height == other.height && width == other.width &&
         is_animated_gif == other.is_animated_gif && is_cta == other.is_cta;
}

bool DoodleImage::operator!=(const DoodleImage& other) const {
  return !(*this == other);
}

DoodleConfig::DoodleConfig() : doodle_type(DoodleType::UNKNOWN) {}
DoodleConfig::DoodleConfig(const DoodleConfig& config) = default;
DoodleConfig::~DoodleConfig() = default;

// static
base::Optional<DoodleConfig> DoodleConfig::FromDictionary(
    const base::DictionaryValue& dict,
    const base::Optional<GURL>& base_url) {
  DoodleConfig doodle;

  // The |large_image| field is required (it's the "default" representation for
  // the doodle).
  base::Optional<DoodleImage> large_image =
      ParseImage(dict, "large_image", base_url);
  if (!large_image.has_value()) {
    return base::nullopt;
  }
  doodle.large_image = large_image.value();

  std::string type_str;
  dict.GetString("doodle_type", &type_str);
  doodle.doodle_type = DoodleTypeFromString(type_str);

  dict.GetString("alt_text", &doodle.alt_text);

  dict.GetString("interactive_html", &doodle.interactive_html);

  doodle.search_url = ParseUrl(dict, "search_url", base_url);
  doodle.target_url = ParseUrl(dict, "target_url", base_url);
  doodle.fullpage_interactive_url =
      ParseUrl(dict, "fullpage_interactive_url", base_url);

  auto large_cta_image = ParseImage(dict, "large_cta_image", base_url);
  if (large_cta_image.has_value()) {
    doodle.large_cta_image = large_cta_image.value();
  }
  auto transparent_large_image =
      ParseImage(dict, "transparent_large_image", base_url);
  if (transparent_large_image.has_value()) {
    doodle.transparent_large_image = transparent_large_image.value();
  }

  return doodle;
}

bool DoodleConfig::operator==(const DoodleConfig& other) const {
  return doodle_type == other.doodle_type && alt_text == other.alt_text &&
         interactive_html == other.interactive_html &&
         search_url == other.search_url && target_url == other.target_url &&
         fullpage_interactive_url == other.fullpage_interactive_url &&
         large_image == other.large_image &&
         large_cta_image == other.large_cta_image &&
         transparent_large_image == other.transparent_large_image;
}

bool DoodleConfig::operator!=(const DoodleConfig& other) const {
  return !(*this == other);
}

}  // namespace doodle
