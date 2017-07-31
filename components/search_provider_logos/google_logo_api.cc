// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/google_logo_api.h"

#include <stdint.h>

#include <algorithm>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/search_provider_logos/features.h"
#include "url/url_constants.h"

namespace search_provider_logos {

GURL GetGoogleDoodleURL(const GURL& google_base_url) {
  GURL::Replacements replacements;
  replacements.SetPathStr(base::FeatureList::IsEnabled(features::kUseDdljsonApi)
                              ? "async/ddljson"
                              : "async/newtab_mobile");
  return google_base_url.ReplaceComponents(replacements);
}

AppendQueryparamsToLogoURL GetGoogleAppendQueryparamsCallback(
    bool gray_background) {
  if (base::FeatureList::IsEnabled(features::kUseDdljsonApi))
    return base::Bind(&GoogleNewAppendQueryparamsToLogoURL, gray_background);

  return base::Bind(&GoogleLegacyAppendQueryparamsToLogoURL, gray_background);
}

ParseLogoResponse GetGoogleParseLogoResponseCallback(const GURL& base_url) {
  if (base::FeatureList::IsEnabled(features::kUseDdljsonApi))
    return base::Bind(&GoogleNewParseLogoResponse, base_url);

  return base::Bind(&GoogleLegacyParseLogoResponse);
}

namespace {
const char kResponsePreamble[] = ")]}'";
}

GURL GoogleLegacyAppendQueryparamsToLogoURL(bool gray_background,
                                            const GURL& logo_url,
                                            const std::string& fingerprint) {
  // Note: we can't just use net::AppendQueryParameter() because it escapes
  // ":" to "%3A", but the server requires the colon not to be escaped.
  // See: http://crbug.com/413845

  // TODO(newt): Switch to using net::AppendQueryParameter once it no longer
  // escapes ":"
  std::string query(logo_url.query());
  if (!query.empty())
    query += "&";

  query += "async=";
  std::vector<base::StringPiece> params;
  std::string fingerprint_param;
  if (!fingerprint.empty()) {
    fingerprint_param = "es_dfp:" + fingerprint;
    params.push_back(fingerprint_param);
  }

  params.push_back("cta:1");

  if (gray_background) {
    params.push_back("transp:1");
    params.push_back("graybg:1");
  }

  query += base::JoinString(params, ",");
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return logo_url.ReplaceComponents(replacements);
}

std::unique_ptr<EncodedLogo> GoogleLegacyParseLogoResponse(
    std::unique_ptr<std::string> response,
    base::Time response_time,
    bool* parsing_failed) {
  // Google doodles are sent as JSON with a prefix. Example:
  //   )]}' {"update":{"logo":{
  //     "data": "/9j/4QAYRXhpZgAASUkqAAgAAAAAAAAAAAAAAP/...",
  //     "mime_type": "image/png",
  //     "fingerprint": "db063e32",
  //     "target": "http://www.google.com.au/search?q=Wilbur+Christiansen",
  //     "url": "http://www.google.com/logos/doodle.png",
  //     "alt": "Wilbur Christiansen's Birthday"
  //     "time_to_live": 1389304799
  //   }}}

  // The response may start with )]}'. Ignore this.
  base::StringPiece response_sp(*response);
  if (response_sp.starts_with(kResponsePreamble))
    response_sp.remove_prefix(strlen(kResponsePreamble));

  // Default parsing failure to be true.
  *parsing_failed = true;

  int error_code;
  std::string error_string;
  int error_line;
  int error_col;
  std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
      response_sp, 0, &error_code, &error_string, &error_line, &error_col);
  if (!value) {
    LOG(WARNING) << error_string << " at " << error_line << ":" << error_col;
    return nullptr;
  }
  // The important data lives inside several nested dictionaries:
  // {"update": {"logo": { "mime_type": ..., etc } } }
  const base::DictionaryValue* outer_dict;
  if (!value->GetAsDictionary(&outer_dict))
    return nullptr;
  const base::DictionaryValue* update_dict;
  if (!outer_dict->GetDictionary("update", &update_dict))
    return nullptr;

  // If there is no logo today, the "update" dictionary will be empty.
  if (update_dict->empty()) {
    *parsing_failed = false;
    return nullptr;
  }

  const base::DictionaryValue* logo_dict;
  if (!update_dict->GetDictionary("logo", &logo_dict))
    return nullptr;

  std::unique_ptr<EncodedLogo> logo(new EncodedLogo());

  std::string encoded_image_base64;
  if (logo_dict->GetString("data", &encoded_image_base64)) {
    // Data is optional, since we may be revalidating a cached logo.
    base::RefCountedString* encoded_image_string = new base::RefCountedString();
    if (!base::Base64Decode(encoded_image_base64,
                            &encoded_image_string->data()))
      return nullptr;
    logo->encoded_image = encoded_image_string;
    if (!logo_dict->GetString("mime_type", &logo->metadata.mime_type))
      return nullptr;
  }

  // Don't check return values since these fields are optional.
  std::string on_click_url;
  logo_dict->GetString("target", &on_click_url);
  logo->metadata.on_click_url = GURL(on_click_url);
  logo_dict->GetString("fingerprint", &logo->metadata.fingerprint);
  logo_dict->GetString("alt", &logo->metadata.alt_text);

  // Existance of url indicates |data| is a call to action image for an
  // animated doodle. |url| points to that animated doodle.
  std::string animated_url;
  logo_dict->GetString("url", &animated_url);
  logo->metadata.animated_url = GURL(animated_url);

  base::TimeDelta time_to_live;
  int time_to_live_ms;
  if (logo_dict->GetInteger("time_to_live", &time_to_live_ms)) {
    time_to_live = base::TimeDelta::FromMilliseconds(
        std::min(static_cast<int64_t>(time_to_live_ms), kMaxTimeToLiveMS));
    logo->metadata.can_show_after_expiration = false;
  } else {
    time_to_live = base::TimeDelta::FromMilliseconds(kMaxTimeToLiveMS);
    logo->metadata.can_show_after_expiration = true;
  }
  logo->metadata.expiration_time = response_time + time_to_live;

  // If this point is reached, parsing has succeeded.
  *parsing_failed = false;
  return logo;
}

GURL GoogleNewAppendQueryparamsToLogoURL(bool gray_background,
                                         const GURL& logo_url,
                                         const std::string& fingerprint) {
  // Note: we can't just use net::AppendQueryParameter() because it escapes
  // ":" to "%3A", but the server requires the colon not to be escaped.
  // See: http://crbug.com/413845

  std::string query(logo_url.query());
  if (!query.empty())
    query += "&";

  query += "async=";

  std::vector<base::StringPiece> params;
  params.push_back("ntp:1");
  if (gray_background) {
    params.push_back("graybg:1");
  }
  if (!fingerprint.empty()) {
    params.push_back("es_dfp:" + fingerprint);
  }
  query += base::JoinString(params, ",");

  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return logo_url.ReplaceComponents(replacements);
}

namespace {

GURL ParseUrl(const base::DictionaryValue& parent_dict,
              const std::string& key,
              const GURL& base_url) {
  std::string url_str;
  if (!parent_dict.GetString(key, &url_str) || url_str.empty()) {
    return GURL();
  }
  return base_url.Resolve(url_str);
}

}  // namespace

std::unique_ptr<EncodedLogo> GoogleNewParseLogoResponse(
    const GURL& base_url,
    std::unique_ptr<std::string> response,
    base::Time response_time,
    bool* parsing_failed) {
  // The response may start with )]}'. Ignore this.
  base::StringPiece response_sp(*response);
  if (response_sp.starts_with(kResponsePreamble))
    response_sp.remove_prefix(strlen(kResponsePreamble));

  // Default parsing failure to be true.
  *parsing_failed = true;

  int error_code;
  std::string error_string;
  int error_line;
  int error_col;
  std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
      response_sp, 0, &error_code, &error_string, &error_line, &error_col);
  if (!value) {
    LOG(WARNING) << error_string << " at " << error_line << ":" << error_col;
    return nullptr;
  }

  std::unique_ptr<base::DictionaryValue> config =
      base::DictionaryValue::From(std::move(value));
  if (!config)
    return nullptr;

  const base::DictionaryValue* ddljson = nullptr;
  if (!config->GetDictionary("ddljson", &ddljson))
    return nullptr;

  // If there is no logo today, the "ddljson" dictionary will be empty.
  if (ddljson->empty()) {
    *parsing_failed = false;
    return nullptr;
  }

  auto logo = base::MakeUnique<EncodedLogo>();

  // Check if the main image is animated.
  bool is_animated = false;
  const base::DictionaryValue* image = nullptr;
  if (ddljson->GetDictionary("large_image", &image)) {
    image->GetBoolean("is_animated_gif", &is_animated);

    // If animated, get the URL for the animated image.
    if (is_animated) {
      logo->metadata.animated_url = ParseUrl(*image, "url", base_url);
      if (!logo->metadata.animated_url.is_valid())
        return nullptr;
    }
  }

  // Data is optional, since we may be revalidating a cached logo.
  // If this is an animated doodle, get the CTA image data.
  std::string encoded_image_data;
  if (ddljson->GetString(is_animated ? "cta_data_uri" : "data_uri",
                         &encoded_image_data)) {
    GURL encoded_image_uri(encoded_image_data);
    if (!encoded_image_uri.is_valid() ||
        !encoded_image_uri.SchemeIs(url::kDataScheme)) {
      return nullptr;
    }
    std::string content = encoded_image_uri.GetContent();
    // The content should look like this: "image/png;base64,aaa..." (where
    // "aaa..." is the base64-encoded image data).
    size_t mime_type_end = content.find_first_of(';');
    if (mime_type_end == std::string::npos)
      return nullptr;
    logo->metadata.mime_type = content.substr(0, mime_type_end);

    size_t base64_begin = mime_type_end + 1;
    size_t base64_end = content.find_first_of(',', base64_begin);
    if (base64_end == std::string::npos)
      return nullptr;
    base::StringPiece base64(content.begin() + base64_begin,
                             content.begin() + base64_end);
    if (base64 != "base64")
      return nullptr;

    size_t data_begin = base64_end + 1;
    base::StringPiece data(content.begin() + data_begin, content.end());
    logo->encoded_image = base::MakeRefCounted<base::RefCountedString>();
    if (!base::Base64Decode(data, &logo->encoded_image->data()))
      return nullptr;
  }

  logo->metadata.on_click_url = ParseUrl(*ddljson, "target_url", base_url);
  ddljson->GetString("alt_text", &logo->metadata.alt_text);

  ddljson->GetString("fingerprint", &logo->metadata.fingerprint);

  base::TimeDelta time_to_live;
  // The JSON doesn't guarantee the number to fit into an int.
  double ttl_ms = 0;  // Expires immediately if the parameter is missing.
  if (ddljson->GetDouble("time_to_live_ms", &ttl_ms)) {
    time_to_live = base::TimeDelta::FromMillisecondsD(ttl_ms);
    logo->metadata.can_show_after_expiration = false;
  } else {
    time_to_live = base::TimeDelta::FromMilliseconds(kMaxTimeToLiveMS);
    logo->metadata.can_show_after_expiration = true;
  }
  logo->metadata.expiration_time = response_time + time_to_live;

  *parsing_failed = false;
  return logo;
}

}  // namespace search_provider_logos
