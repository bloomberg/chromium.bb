// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_types.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace doodle {

namespace {

// String representations for the DoodleType enum.
const char kDoodleTypeUnknown[] = "UNKNOWN";
const char kDoodleTypeSimple[] = "SIMPLE";
const char kDoodleTypeRandom[] = "RANDOM";
const char kDoodleTypeVideo[] = "VIDEO";
const char kDoodleTypeInteractive[] = "INTERACTIVE";
const char kDoodleTypeInlineInteractive[] = "INLINE_INTERACTIVE";
const char kDoodleTypeSlideshow[] = "SLIDESHOW";

// JSON keys for DoodleImage fields.
const char kKeyUrl[] = "url";

// JSON keys for DoodleConfig fields.
const char kKeyDoodleType[] = "doodle_type";
const char kKeyAltText[] = "alt_text";
const char kKeyTargetUrl[] = "target_url";
const char kKeyLargeImage[] = "large_image";
const char kKeyLargeCtaImage[] = "large_cta_image";

std::string DoodleTypeToString(DoodleType type) {
  switch (type) {
    case DoodleType::UNKNOWN:
      return kDoodleTypeUnknown;
    case DoodleType::SIMPLE:
      return kDoodleTypeSimple;
    case DoodleType::RANDOM:
      return kDoodleTypeRandom;
    case DoodleType::VIDEO:
      return kDoodleTypeVideo;
    case DoodleType::INTERACTIVE:
      return kDoodleTypeInteractive;
    case DoodleType::INLINE_INTERACTIVE:
      return kDoodleTypeInlineInteractive;
    case DoodleType::SLIDESHOW:
      return kDoodleTypeSlideshow;
  }
  NOTREACHED();
  return std::string();
}

DoodleType DoodleTypeFromString(const std::string& type_str) {
  if (type_str == kDoodleTypeSimple) {
    return DoodleType::SIMPLE;
  }
  if (type_str == kDoodleTypeRandom) {
    return DoodleType::RANDOM;
  }
  if (type_str == kDoodleTypeVideo) {
    return DoodleType::VIDEO;
  }
  if (type_str == kDoodleTypeInteractive) {
    return DoodleType::INTERACTIVE;
  }
  if (type_str == kDoodleTypeInlineInteractive) {
    return DoodleType::INLINE_INTERACTIVE;
  }
  if (type_str == kDoodleTypeSlideshow) {
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
  // The URL is the only required field.
  GURL url = ParseUrl(dict, kKeyUrl, base_url);
  if (!url.is_valid()) {
    return base::nullopt;
  }

  return DoodleImage(url);
}

DoodleImage::DoodleImage(const GURL& url) : url(url) {
  DCHECK(url.is_valid());
}

DoodleImage::~DoodleImage() = default;

std::unique_ptr<base::DictionaryValue> DoodleImage::ToDictionary() const {
  auto dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetString(kKeyUrl, url.spec());
  return dict;
}

bool DoodleImage::operator==(const DoodleImage& other) const {
  return url == other.url;
}

bool DoodleImage::operator!=(const DoodleImage& other) const {
  return !(*this == other);
}

DoodleConfig::DoodleConfig(DoodleType doodle_type,
                           const DoodleImage& large_image)
    : doodle_type(doodle_type), large_image(large_image) {}
DoodleConfig::DoodleConfig(const DoodleConfig& config) = default;
DoodleConfig::~DoodleConfig() = default;

// static
base::Optional<DoodleConfig> DoodleConfig::FromDictionary(
    const base::DictionaryValue& dict,
    const base::Optional<GURL>& base_url) {
  // The |large_image| field is required (it's the "default" representation for
  // the doodle).
  base::Optional<DoodleImage> large_image =
      ParseImage(dict, kKeyLargeImage, base_url);
  if (!large_image.has_value()) {
    return base::nullopt;
  }

  std::string type_str;
  dict.GetString(kKeyDoodleType, &type_str);

  DoodleConfig doodle(DoodleTypeFromString(type_str), large_image.value());

  dict.GetString(kKeyAltText, &doodle.alt_text);

  doodle.target_url = ParseUrl(dict, kKeyTargetUrl, base_url);

  doodle.large_cta_image = ParseImage(dict, kKeyLargeCtaImage, base_url);

  return doodle;
}

std::unique_ptr<base::DictionaryValue> DoodleConfig::ToDictionary() const {
  auto dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetString(kKeyDoodleType, DoodleTypeToString(doodle_type));
  dict->SetString(kKeyAltText, alt_text);
  dict->SetString(kKeyTargetUrl, target_url.spec());
  dict->Set(kKeyLargeImage, large_image.ToDictionary());
  if (large_cta_image.has_value()) {
    dict->Set(kKeyLargeCtaImage, large_cta_image->ToDictionary());
  }
  return dict;
}

bool DoodleConfig::operator==(const DoodleConfig& other) const {
  return doodle_type == other.doodle_type && alt_text == other.alt_text &&
         large_image == other.large_image &&
         large_cta_image == other.large_cta_image;
}

bool DoodleConfig::operator!=(const DoodleConfig& other) const {
  return !(*this == other);
}

}  // namespace doodle
