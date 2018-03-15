// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/manifest_share_target_util.h"

#include <map>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace content {
namespace {

// Determines whether a character is allowed in a URL template placeholder.
bool IsIdentifier(char c) {
  return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '-' || c == '_';
}

// Returns to |out| the result of running the "replace placeholders" algorithm
// on |template_string|. The algorithm is specified at
// https://wicg.github.io/web-share-target/#dfn-replace-placeholders
bool ReplacePlaceholders(base::StringPiece template_string,
                         base::StringPiece title,
                         base::StringPiece text,
                         const GURL& share_url,
                         std::string* out) {
  constexpr char kTitlePlaceholder[] = "title";
  constexpr char kTextPlaceholder[] = "text";
  constexpr char kUrlPlaceholder[] = "url";

  std::map<base::StringPiece, std::string> placeholder_to_data;
  placeholder_to_data[kTitlePlaceholder] =
      net::EscapeQueryParamValue(title, false);
  placeholder_to_data[kTextPlaceholder] =
      net::EscapeQueryParamValue(text, false);
  placeholder_to_data[kUrlPlaceholder] =
      net::EscapeQueryParamValue(share_url.spec(), false);

  std::vector<base::StringPiece> split_template;
  bool last_saw_open = false;
  size_t start_index_to_copy = 0;
  for (size_t i = 0; i < template_string.size(); ++i) {
    if (last_saw_open) {
      if (template_string[i] == '}') {
        base::StringPiece placeholder = template_string.substr(
            start_index_to_copy + 1, i - 1 - start_index_to_copy);
        auto it = placeholder_to_data.find(placeholder);
        if (it != placeholder_to_data.end()) {
          // Replace the placeholder text with the parameter value.
          split_template.push_back(it->second);
        }

        last_saw_open = false;
        start_index_to_copy = i + 1;
      } else if (!IsIdentifier(template_string[i])) {
        // Error: Non-identifier character seen after open.
        return false;
      }
    } else {
      if (template_string[i] == '}') {
        // Error: Saw close, with no corresponding open.
        return false;
      } else if (template_string[i] == '{') {
        split_template.push_back(template_string.substr(
            start_index_to_copy, i - start_index_to_copy));

        last_saw_open = true;
        start_index_to_copy = i;
      }
    }
  }
  if (last_saw_open) {
    // Error: Saw open that was never closed.
    return false;
  }
  split_template.push_back(template_string.substr(
      start_index_to_copy, template_string.size() - start_index_to_copy));

  *out = base::StrCat(split_template);
  return true;
}

}  // namespace

bool ReplaceWebShareUrlPlaceholders(const GURL& url_template,
                                    base::StringPiece title,
                                    base::StringPiece text,
                                    const GURL& share_url,
                                    GURL* url_template_filled) {
  std::string new_query;
  std::string new_ref;
  if (!ReplacePlaceholders(url_template.query_piece(), title, text, share_url,
                           &new_query) ||
      !ReplacePlaceholders(url_template.ref_piece(), title, text, share_url,
                           &new_ref)) {
    return false;
  }

  // Check whether |url_template| has a query in order to preserve the '?' in a
  // URL with an empty query. e.g. http://www.google.com/?
  GURL::Replacements url_replacements;
  if (url_template.has_query())
    url_replacements.SetQueryStr(new_query);
  if (url_template.has_ref())
    url_replacements.SetRefStr(new_ref);
  *url_template_filled = url_template.ReplaceComponents(url_replacements);
  return true;
}

}  // namespace content
