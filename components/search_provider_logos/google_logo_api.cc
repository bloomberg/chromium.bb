// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/google_logo_api.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/base/url_util.h"

namespace search_provider_logos {

namespace {
const char kResponsePreamble[] = ")]}'";
}

GURL GoogleAppendFingerprintToLogoURL(const GURL& logo_url,
                                      const std::string& fingerprint) {
  return net::AppendQueryParameter(logo_url, "async", "es_dfp:" + fingerprint);
}

scoped_ptr<EncodedLogo> GoogleParseLogoResponse(
    const scoped_ptr<std::string>& response,
    base::Time response_time) {
  // Google doodles are sent as JSON with a prefix. Example:
  //   )]}' {"update":{"logo":{
  //     "data": "/9j/4QAYRXhpZgAASUkqAAgAAAAAAAAAAAAAAP/...",
  //     "mime_type": "image/png",
  //     "fingerprint": "db063e32",
  //     "target": "http://www.google.com.au/search?q=Wilbur+Christiansen",
  //     "alt": "Wilbur Christiansen's Birthday"
  //     "time_to_live": 1389304799
  //   }}}

  // The response may start with )]}'. Ignore this.
  base::StringPiece response_sp(*response);
  if (response_sp.starts_with(kResponsePreamble))
    response_sp.remove_prefix(strlen(kResponsePreamble));

  scoped_ptr<base::Value> value(base::JSONReader::Read(response_sp));
  if (!value.get())
    return scoped_ptr<EncodedLogo>();

  // The important data lives inside several nested dictionaries:
  // {"update": {"logo": { "mime_type": ..., etc } } }
  const base::DictionaryValue* outer_dict;
  if (!value->GetAsDictionary(&outer_dict))
    return scoped_ptr<EncodedLogo>();
  const base::DictionaryValue* update_dict;
  if (!outer_dict->GetDictionary("update", &update_dict))
    return scoped_ptr<EncodedLogo>();
  const base::DictionaryValue* logo_dict;
  if (!update_dict->GetDictionary("logo", &logo_dict))
    return scoped_ptr<EncodedLogo>();

  scoped_ptr<EncodedLogo> logo(new EncodedLogo());

  std::string encoded_image_base64;
  if (logo_dict->GetString("data", &encoded_image_base64)) {
    // Data is optional, since we may be revalidating a cached logo.
    base::RefCountedString* encoded_image_string = new base::RefCountedString();
    if (!base::Base64Decode(encoded_image_base64,
                            &encoded_image_string->data()))
      return scoped_ptr<EncodedLogo>();
    logo->encoded_image = encoded_image_string;
    if (!logo_dict->GetString("mime_type", &logo->metadata.mime_type))
      return scoped_ptr<EncodedLogo>();
  }

  // Don't check return values since these fields are optional.
  logo_dict->GetString("target", &logo->metadata.on_click_url);
  logo_dict->GetString("fingerprint", &logo->metadata.fingerprint);
  logo_dict->GetString("alt", &logo->metadata.alt_text);

  base::TimeDelta time_to_live;
  int time_to_live_ms;
  if (logo_dict->GetInteger("time_to_live", &time_to_live_ms)) {
    time_to_live = base::TimeDelta::FromMilliseconds(
        std::min(static_cast<int64>(time_to_live_ms), kMaxTimeToLiveMS));
    logo->metadata.can_show_after_expiration = false;
  } else {
    time_to_live = base::TimeDelta::FromMilliseconds(kMaxTimeToLiveMS);
    logo->metadata.can_show_after_expiration = true;
  }
  logo->metadata.expiration_time = response_time + time_to_live;

  return logo.Pass();
}

}  // namespace search_provider_logos
