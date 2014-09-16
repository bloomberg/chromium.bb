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

namespace {

// Parses the 'name' field of the manifest, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-name-member
// Returns the parsed string if any, a null string if the parsing failed.
base::NullableString16 ParseName(const base::DictionaryValue& dictionary)  {
  if (!dictionary.HasKey("name"))
    return base::NullableString16();

  base::string16 name;
  if (!dictionary.GetString("name", &name)) {
    // TODO(mlamouri): provide a custom message to the developer console.
    return base::NullableString16();
  }

  base::TrimWhitespace(name, base::TRIM_ALL, &name);
  return base::NullableString16(name, false);
}

// Parses the 'short_name' field of the manifest, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-short-name-member
// Returns the parsed string if any, a null string if the parsing failed.
base::NullableString16 ParseShortName(
    const base::DictionaryValue& dictionary)  {
  if (!dictionary.HasKey("short_name"))
    return base::NullableString16();

  base::string16 short_name;
  if (!dictionary.GetString("short_name", &short_name)) {
    // TODO(mlamouri): provide a custom message to the developer console.
    return base::NullableString16();
  }

  base::TrimWhitespace(short_name, base::TRIM_ALL, &short_name);
  return base::NullableString16(short_name, false);
}

// Parses the 'start_url' field of the manifest, as defined in:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-the-start_url-member
// Returns the parsed GURL if any, an empty GURL if the parsing failed.
GURL ParseStartURL(const base::DictionaryValue& dictionary,
                   const GURL& manifest_url,
                   const GURL& document_url) {
  if (!dictionary.HasKey("start_url"))
    return GURL();

  base::string16 start_url_str;
  if (!dictionary.GetString("start_url", &start_url_str)) {
    // TODO(mlamouri): provide a custom message to the developer console.
    return GURL();
  }

  GURL start_url = manifest_url.Resolve(start_url_str);
  if (!start_url.is_valid())
    return GURL();

  if (start_url.GetOrigin() != document_url.GetOrigin()) {
    // TODO(mlamouri): provide a custom message to the developer console.
    return GURL();
  }

  return start_url;
}

} // anonymous namespace

namespace content {

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

  return manifest;
}

} // namespace content
