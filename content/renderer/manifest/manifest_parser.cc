// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/manifest/manifest_parser.h"

#include "base/json/json_reader.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/common/manifest.h"

namespace content {

namespace {

enum TrimType {
  Trim,
  NoTrim
};

base::NullableString16 ParseString(const base::DictionaryValue& dictionary,
                                   const std::string& key,
                                   TrimType trim) {
  if (!dictionary.HasKey(key))
    return base::NullableString16();

  base::string16 value;
  if (!dictionary.GetString(key, &value)) {
    // TODO(mlamouri): provide a custom message to the developer console about
    // the property being incorrectly set.
    return base::NullableString16();
  }

  if (trim == Trim)
    base::TrimWhitespace(value, base::TRIM_ALL, &value);
  return base::NullableString16(value, false);
}

// Parses the 'name' field of the manifest, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-name-member
// Returns the parsed string if any, a null string if the parsing failed.
base::NullableString16 ParseName(const base::DictionaryValue& dictionary)  {
  return ParseString(dictionary, "name", Trim);
}

// Parses the 'short_name' field of the manifest, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-short-name-member
// Returns the parsed string if any, a null string if the parsing failed.
base::NullableString16 ParseShortName(
    const base::DictionaryValue& dictionary)  {
  return ParseString(dictionary, "short_name", Trim);
}

// Parses the 'start_url' field of the manifest, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-start_url-member
// Returns the parsed GURL if any, an empty GURL if the parsing failed.
GURL ParseStartURL(const base::DictionaryValue& dictionary,
                   const GURL& manifest_url,
                   const GURL& document_url) {
  base::NullableString16 start_url_str =
      ParseString(dictionary, "start_url", NoTrim);

  if (start_url_str.is_null())
    return GURL();

  GURL start_url = manifest_url.Resolve(start_url_str.string());
  if (!start_url.is_valid())
    return GURL();

  if (start_url.GetOrigin() != document_url.GetOrigin()) {
    // TODO(mlamouri): provide a custom message to the developer console.
    return GURL();
  }

  return start_url;
}

// Parses the 'display' field of the manifest, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-display-member
// Returns the parsed DisplayMode if any, DISPLAY_MODE_UNSPECIFIED if the
// parsing failed.
Manifest::DisplayMode ParseDisplay(const base::DictionaryValue& dictionary) {
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
  else
    return Manifest::DISPLAY_MODE_UNSPECIFIED;
}

} // anonymous namespace

Manifest ManifestParser::Parse(const base::StringPiece& json,
                               const GURL& manifest_url,
                               const GURL& document_url) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(json));
  if (!value) {
    // TODO(mlamouri): get the JSON parsing error and report it to the developer
    // console.
    return Manifest();
  }

  if (value->GetType() != base::Value::TYPE_DICTIONARY) {
    // TODO(mlamouri): provide a custom message to the developer console.
    return Manifest();
  }

  base::DictionaryValue* dictionary = 0;
  value->GetAsDictionary(&dictionary);
  if (!dictionary) {
    // TODO(mlamouri): provide a custom message to the developer console.
    return Manifest();
  }

  Manifest manifest;

  manifest.name = ParseName(*dictionary);
  manifest.short_name = ParseShortName(*dictionary);
  manifest.start_url = ParseStartURL(*dictionary, manifest_url, document_url);
  manifest.display = ParseDisplay(*dictionary);

  return manifest;
}

} // namespace content
