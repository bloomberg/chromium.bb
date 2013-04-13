// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/url_util.h"

#include "base/strings/string_piece.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

namespace net {

GURL AppendQueryParameter(const GURL& url,
                          const std::string& name,
                          const std::string& value) {
  std::string query(url.query());

  if (!query.empty())
    query += "&";

  query += (EscapeQueryParamValue(name, true) + "=" +
            EscapeQueryParamValue(value, true));
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

GURL AppendOrReplaceQueryParameter(const GURL& url,
                                   const std::string& name,
                                   const std::string& value) {
  bool replaced = false;
  std::string param_name = EscapeQueryParamValue(name, true);
  std::string param_value = EscapeQueryParamValue(value, true);

  const std::string input = url.query();
  url_parse::Component cursor(0, input.size());
  std::string output;
  url_parse::Component key_range, value_range;
  while (url_parse::ExtractQueryKeyValue(
             input.data(), &cursor, &key_range, &value_range)) {
    const base::StringPiece key(
        input.data() + key_range.begin, key_range.len);
    const base::StringPiece value(
        input.data() + value_range.begin, value_range.len);
    std::string key_value_pair;
    // Check |replaced| as only the first pair should be replaced.
    if (!replaced && key == param_name) {
      replaced = true;
      key_value_pair = (param_name + "=" + param_value);
    } else {
      key_value_pair.assign(input.data(),
                            key_range.begin,
                            value_range.end() - key_range.begin);
    }
    if (!output.empty())
      output += "&";

    output += key_value_pair;
  }
  if (!replaced) {
    if (!output.empty())
      output += "&";

    output += (param_name + "=" + param_value);
  }
  GURL::Replacements replacements;
  replacements.SetQueryStr(output);
  return url.ReplaceComponents(replacements);
}

bool GetValueForKeyInQuery(const GURL& url,
                           const std::string& search_key,
                           std::string* out_value) {
  url_parse::Component query = url.parsed_for_possibly_invalid_spec().query;
  url_parse::Component key, value;
  while (url_parse::ExtractQueryKeyValue(
      url.spec().c_str(), &query, &key, &value)) {
    if (key.is_nonempty()) {
      std::string key_string = url.spec().substr(key.begin, key.len);
      if (key_string == search_key) {
        if (value.is_nonempty()) {
          *out_value = UnescapeURLComponent(
              url.spec().substr(value.begin, value.len),
              UnescapeRule::SPACES |
              UnescapeRule::URL_SPECIAL_CHARS |
              UnescapeRule::REPLACE_PLUS_WITH_SPACE);
        } else {
          *out_value = "";
        }
        return true;
      }
    }
  }
  return false;
}

}  // namespace net
