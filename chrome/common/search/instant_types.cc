// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/search/instant_types.h"

#include "base/strings/utf_string_conversions.h"
#include "net/base/escape.h"

namespace {

std::string GetComponent(const std::string& url,
                         const url::Component component) {
  return (component.len > 0) ? url.substr(component.begin, component.len) :
      std::string();
}

}  // namespace

InstantSuggestion::InstantSuggestion() {
}

InstantSuggestion::InstantSuggestion(const base::string16& in_text,
                                     const std::string& in_metadata)
    : text(in_text),
      metadata(in_metadata) {
}

InstantSuggestion::~InstantSuggestion() {
}

RGBAColor::RGBAColor()
    : r(0),
      g(0),
      b(0),
      a(0) {
}

RGBAColor::~RGBAColor() {
}

bool RGBAColor::operator==(const RGBAColor& rhs) const {
  return r == rhs.r &&
      g == rhs.g &&
      b == rhs.b &&
      a == rhs.a;
}

ThemeBackgroundInfo::ThemeBackgroundInfo()
    : using_default_theme(true),
      background_color(),
      text_color(),
      link_color(),
      text_color_light(),
      header_color(),
      section_border_color(),
      image_horizontal_alignment(THEME_BKGRND_IMAGE_ALIGN_CENTER),
      image_vertical_alignment(THEME_BKGRND_IMAGE_ALIGN_CENTER),
      image_tiling(THEME_BKGRND_IMAGE_NO_REPEAT),
      image_height(0),
      has_attribution(false),
      logo_alternate(false) {
}

ThemeBackgroundInfo::~ThemeBackgroundInfo() {
}

bool ThemeBackgroundInfo::operator==(const ThemeBackgroundInfo& rhs) const {
  return using_default_theme == rhs.using_default_theme &&
      background_color == rhs.background_color &&
      text_color == rhs.text_color &&
      link_color == rhs.link_color &&
      text_color_light == rhs.text_color_light &&
      header_color == rhs.header_color &&
      section_border_color == rhs.section_border_color &&
      theme_id == rhs.theme_id &&
      image_horizontal_alignment == rhs.image_horizontal_alignment &&
      image_vertical_alignment == rhs.image_vertical_alignment &&
      image_tiling == rhs.image_tiling &&
      image_height == rhs.image_height &&
      has_attribution == rhs.has_attribution &&
      logo_alternate == rhs.logo_alternate;
}

const char kSearchQueryKey[] = "q";
const char kOriginalQueryKey[] = "oq";
const char kRLZParameterKey[] = "rlz";
const char kInputEncodingKey[] = "ie";
const char kAssistedQueryStatsKey[] = "aqs";

InstantMostVisitedItem::InstantMostVisitedItem()
    : source(ntp_tiles::NTPTileSource::TOP_SITES) {}

InstantMostVisitedItem::InstantMostVisitedItem(
    const InstantMostVisitedItem& other) = default;

InstantMostVisitedItem::~InstantMostVisitedItem() {}

EmbeddedSearchRequestParams::EmbeddedSearchRequestParams() {
}

EmbeddedSearchRequestParams::EmbeddedSearchRequestParams(const GURL& url) {
  const std::string& url_params(url.ref().empty()? url.query() : url.ref());
  url::Component query, key, value;
  query.len = static_cast<int>(url_params.size());

  const net::UnescapeRule::Type unescape_rules =
      net::UnescapeRule::SPOOFING_AND_CONTROL_CHARS |
      net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
      net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
      net::UnescapeRule::NORMAL | net::UnescapeRule::REPLACE_PLUS_WITH_SPACE;

  while (url::ExtractQueryKeyValue(url_params.c_str(), &query, &key, &value)) {
    if (!key.is_nonempty())
      continue;

    std::string key_param(GetComponent(url_params, key));
    std::string value_param(GetComponent(url_params, value));
    if (key_param == kSearchQueryKey) {
      search_query = base::UTF8ToUTF16(net::UnescapeURLComponent(
          value_param, unescape_rules));
    } else if (key_param == kOriginalQueryKey) {
      original_query = base::UTF8ToUTF16(net::UnescapeURLComponent(
          value_param, unescape_rules));
    } else if (key_param == kRLZParameterKey) {
      rlz_parameter_value = net::UnescapeAndDecodeUTF8URLComponent(
          value_param, net::UnescapeRule::NORMAL);
    } else if (key_param == kInputEncodingKey) {
      input_encoding = net::UnescapeAndDecodeUTF8URLComponent(
          value_param, net::UnescapeRule::NORMAL);
    } else if (key_param == kAssistedQueryStatsKey) {
      assisted_query_stats = net::UnescapeAndDecodeUTF8URLComponent(
          value_param, net::UnescapeRule::NORMAL);
    }
  }
}

EmbeddedSearchRequestParams::~EmbeddedSearchRequestParams() {
}
