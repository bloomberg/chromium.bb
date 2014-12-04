// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/manifest/manifest_parser.h"

#include "base/json/json_reader.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/common/manifest.h"
#include "content/renderer/manifest/manifest_uma_util.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

// Helper function that returns whether the given |str| is a valid width or
// height value for an icon sizes per:
// https://html.spec.whatwg.org/multipage/semantics.html#attr-link-sizes
bool IsValidIconWidthOrHeight(const std::string& str) {
  if (str.empty() || str[0] == '0')
    return false;
  for (size_t i = 0; i < str.size(); ++i)
    if (!IsAsciiDigit(str[i]))
      return false;
  return true;
}

// Parses the 'sizes' attribute of an icon as described in the HTML spec:
// https://html.spec.whatwg.org/multipage/semantics.html#attr-link-sizes
// Return a vector of gfx::Size that contains the valid sizes found. "Any" is
// represented by gfx::Size(0, 0).
// TODO(mlamouri): this is implemented as a separate function because it should
// be refactored with the other icon sizes parsing implementations, see
// http://crbug.com/416477
std::vector<gfx::Size> ParseIconSizesHTML(const base::string16& sizes_str16) {
  if (!base::IsStringASCII(sizes_str16))
    return std::vector<gfx::Size>();

  std::vector<gfx::Size> sizes;
  std::string sizes_str =
      base::StringToLowerASCII(base::UTF16ToUTF8(sizes_str16));
  std::vector<std::string> sizes_str_list;
  base::SplitStringAlongWhitespace(sizes_str, &sizes_str_list);

  for (size_t i = 0; i < sizes_str_list.size(); ++i) {
    std::string& size_str = sizes_str_list[i];
    if (size_str == "any") {
      sizes.push_back(gfx::Size(0, 0));
      continue;
    }

    // It is expected that [0] => width and [1] => height after the split.
    std::vector<std::string> size_list;
    base::SplitStringDontTrim(size_str, L'x', &size_list);
    if (size_list.size() != 2)
      continue;
    if (!IsValidIconWidthOrHeight(size_list[0]) ||
        !IsValidIconWidthOrHeight(size_list[1])) {
      continue;
    }

    int width, height;
    if (!base::StringToInt(size_list[0], &width) ||
        !base::StringToInt(size_list[1], &height)) {
      continue;
    }

    sizes.push_back(gfx::Size(width, height));
  }

  return sizes;
}

const std::string& GetErrorPrefix() {
  CR_DEFINE_STATIC_LOCAL(std::string, error_prefix,
                         ("Manifest parsing error: "));
  return error_prefix;
}

}  // anonymous namespace


ManifestParser::ManifestParser(const base::StringPiece& data,
                               const GURL& manifest_url,
                               const GURL& document_url)
    : data_(data),
      manifest_url_(manifest_url),
      document_url_(document_url),
      failed_(false) {
}

ManifestParser::~ManifestParser() {
}

void ManifestParser::Parse() {
  std::string parse_error;
  scoped_ptr<base::Value> value(
      base::JSONReader::ReadAndReturnError(data_, base::JSON_PARSE_RFC,
                                           nullptr, &parse_error));

  if (!value) {
    errors_.push_back(GetErrorPrefix() + parse_error);
    ManifestUmaUtil::ParseFailed();
    failed_ = true;
    return;
  }

  base::DictionaryValue* dictionary = nullptr;
  if (!value->GetAsDictionary(&dictionary)) {
    errors_.push_back(GetErrorPrefix() +
                      "root element must be a valid JSON object.");
    ManifestUmaUtil::ParseFailed();
    failed_ = true;
    return;
  }
  DCHECK(dictionary);

  manifest_.name = ParseName(*dictionary);
  manifest_.short_name = ParseShortName(*dictionary);
  manifest_.start_url = ParseStartURL(*dictionary);
  manifest_.display = ParseDisplay(*dictionary);
  manifest_.orientation = ParseOrientation(*dictionary);
  manifest_.icons = ParseIcons(*dictionary);
  manifest_.gcm_sender_id = ParseGCMSenderID(*dictionary);
  manifest_.gcm_user_visible_only = ParseGCMUserVisibleOnly(*dictionary);

  ManifestUmaUtil::ParseSucceeded(manifest_);
}

const Manifest& ManifestParser::manifest() const {
  return manifest_;
}

const std::vector<std::string>& ManifestParser::errors() const {
  return errors_;
}

bool ManifestParser::failed() const {
  return failed_;
}

bool ManifestParser::ParseBoolean(const base::DictionaryValue& dictionary,
                                  const std::string& key,
                                  bool default_value) {
  if (!dictionary.HasKey(key))
    return default_value;

  bool value;
  if (!dictionary.GetBoolean(key, &value)) {
    errors_.push_back(GetErrorPrefix() +
                      "property '" + key + "' ignored, type boolean expected.");
    return default_value;
  }

  return value;
}

base::NullableString16 ManifestParser::ParseString(
    const base::DictionaryValue& dictionary,
    const std::string& key,
    TrimType trim) {
  if (!dictionary.HasKey(key))
    return base::NullableString16();

  base::string16 value;
  if (!dictionary.GetString(key, &value)) {
    errors_.push_back(GetErrorPrefix() +
                      "property '" + key + "' ignored, type string expected.");
    return base::NullableString16();
  }

  if (trim == Trim)
    base::TrimWhitespace(value, base::TRIM_ALL, &value);
  return base::NullableString16(value, false);
}

GURL ManifestParser::ParseURL(const base::DictionaryValue& dictionary,
                              const std::string& key,
                              const GURL& base_url) {
  base::NullableString16 url_str = ParseString(dictionary, key, NoTrim);
  if (url_str.is_null())
    return GURL();

  return base_url.Resolve(url_str.string());
}

base::NullableString16 ManifestParser::ParseName(
    const base::DictionaryValue& dictionary)  {
  return ParseString(dictionary, "name", Trim);
}

base::NullableString16 ManifestParser::ParseShortName(
    const base::DictionaryValue& dictionary)  {
  return ParseString(dictionary, "short_name", Trim);
}

GURL ManifestParser::ParseStartURL(const base::DictionaryValue& dictionary) {
  GURL start_url = ParseURL(dictionary, "start_url", manifest_url_);
  if (!start_url.is_valid())
    return GURL();

  if (start_url.GetOrigin() != document_url_.GetOrigin()) {
    errors_.push_back(GetErrorPrefix() + "property 'start_url' ignored, should "
                      "be same origin as document.");
    return GURL();
  }

  return start_url;
}

Manifest::DisplayMode ManifestParser::ParseDisplay(
    const base::DictionaryValue& dictionary) {
  base::NullableString16 display = ParseString(dictionary, "display", Trim);
  if (display.is_null())
    return Manifest::DISPLAY_MODE_UNSPECIFIED;

  if (LowerCaseEqualsASCII(display.string(), "fullscreen"))
    return Manifest::DISPLAY_MODE_FULLSCREEN;
  else if (LowerCaseEqualsASCII(display.string(), "standalone"))
    return Manifest::DISPLAY_MODE_STANDALONE;
  else if (LowerCaseEqualsASCII(display.string(), "minimal-ui"))
    return Manifest::DISPLAY_MODE_MINIMAL_UI;
  else if (LowerCaseEqualsASCII(display.string(), "browser"))
    return Manifest::DISPLAY_MODE_BROWSER;
  else {
    errors_.push_back(GetErrorPrefix() + "unknown 'display' value ignored.");
    return Manifest::DISPLAY_MODE_UNSPECIFIED;
  }
}

blink::WebScreenOrientationLockType ManifestParser::ParseOrientation(
    const base::DictionaryValue& dictionary) {
  base::NullableString16 orientation =
      ParseString(dictionary, "orientation", Trim);

  if (orientation.is_null())
    return blink::WebScreenOrientationLockDefault;

  if (LowerCaseEqualsASCII(orientation.string(), "any"))
    return blink::WebScreenOrientationLockAny;
  else if (LowerCaseEqualsASCII(orientation.string(), "natural"))
    return blink::WebScreenOrientationLockNatural;
  else if (LowerCaseEqualsASCII(orientation.string(), "landscape"))
    return blink::WebScreenOrientationLockLandscape;
  else if (LowerCaseEqualsASCII(orientation.string(), "landscape-primary"))
    return blink::WebScreenOrientationLockLandscapePrimary;
  else if (LowerCaseEqualsASCII(orientation.string(), "landscape-secondary"))
    return blink::WebScreenOrientationLockLandscapeSecondary;
  else if (LowerCaseEqualsASCII(orientation.string(), "portrait"))
    return blink::WebScreenOrientationLockPortrait;
  else if (LowerCaseEqualsASCII(orientation.string(), "portrait-primary"))
    return blink::WebScreenOrientationLockPortraitPrimary;
  else if (LowerCaseEqualsASCII(orientation.string(), "portrait-secondary"))
    return blink::WebScreenOrientationLockPortraitSecondary;
  else {
    errors_.push_back(GetErrorPrefix() +
                      "unknown 'orientation' value ignored.");
    return blink::WebScreenOrientationLockDefault;
  }
}

GURL ManifestParser::ParseIconSrc(const base::DictionaryValue& icon) {
  return ParseURL(icon, "src", manifest_url_);
}

base::NullableString16 ManifestParser::ParseIconType(
    const base::DictionaryValue& icon) {
  return ParseString(icon, "type", Trim);
}

double ManifestParser::ParseIconDensity(const base::DictionaryValue& icon) {
  double density;
  if (!icon.HasKey("density"))
    return Manifest::Icon::kDefaultDensity;

  if (!icon.GetDouble("density", &density) || density <= 0) {
    errors_.push_back(GetErrorPrefix() +
                      "icon 'density' ignored, must be float greater than 0.");
    return Manifest::Icon::kDefaultDensity;
  }
  return density;
}

std::vector<gfx::Size> ManifestParser::ParseIconSizes(
    const base::DictionaryValue& icon) {
  base::NullableString16 sizes_str = ParseString(icon, "sizes", NoTrim);

  if (sizes_str.is_null())
    return std::vector<gfx::Size>();

  std::vector<gfx::Size> sizes = ParseIconSizesHTML(sizes_str.string());
  if (sizes.empty()) {
    errors_.push_back(GetErrorPrefix() + "found icon with no valid size.");
  }
  return sizes;
}

std::vector<Manifest::Icon> ManifestParser::ParseIcons(
    const base::DictionaryValue& dictionary) {
  std::vector<Manifest::Icon> icons;
  if (!dictionary.HasKey("icons"))
    return icons;

  const base::ListValue* icons_list = nullptr;
  if (!dictionary.GetList("icons", &icons_list)) {
    errors_.push_back(GetErrorPrefix() +
                      "property 'icons' ignored, type array expected.");
    return icons;
  }

  for (size_t i = 0; i < icons_list->GetSize(); ++i) {
    const base::DictionaryValue* icon_dictionary = nullptr;
    if (!icons_list->GetDictionary(i, &icon_dictionary))
      continue;

    Manifest::Icon icon;
    icon.src = ParseIconSrc(*icon_dictionary);
    // An icon MUST have a valid src. If it does not, it MUST be ignored.
    if (!icon.src.is_valid())
      continue;
    icon.type = ParseIconType(*icon_dictionary);
    icon.density = ParseIconDensity(*icon_dictionary);
    icon.sizes = ParseIconSizes(*icon_dictionary);

    icons.push_back(icon);
  }

  return icons;
}

base::NullableString16 ManifestParser::ParseGCMSenderID(
    const base::DictionaryValue& dictionary)  {
  return ParseString(dictionary, "gcm_sender_id", Trim);
}

bool ManifestParser::ParseGCMUserVisibleOnly(
    const base::DictionaryValue& dictionary) {
  return ParseBoolean(dictionary, "gcm_user_visible_only", false);
}

} // namespace content
