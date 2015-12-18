// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippet.h"
#include "base/values.h"

namespace ntp_snippets {

NTPSnippet::NTPSnippet(const GURL& url) : url_(url) {
  DCHECK(url_.is_valid() && !url.is_empty());
}

NTPSnippet::~NTPSnippet() {}

// static
std::unique_ptr<NTPSnippet> NTPSnippet::NTPSnippetFromDictionary(
    const base::DictionaryValue& dict) {
  // Need at least the url.
  std::string url;
  if (!dict.GetString("url", &url))
    return nullptr;

  std::unique_ptr<NTPSnippet> snippet(new NTPSnippet(GURL(url)));

  std::string site_title;
  if (dict.GetString("site_title", &site_title))
    snippet->set_site_title(site_title);
  std::string favicon_url;
  if (dict.GetString("favicon_url", &favicon_url))
    snippet->set_favicon_url(GURL(favicon_url));
  std::string title;
  if (dict.GetString("title", &title))
    snippet->set_title(title);
  std::string snippet_str;
  if (dict.GetString("snippet", &snippet_str))
    snippet->set_snippet(snippet_str);
  std::string salient_image_url;
  if (dict.GetString("thumbnailUrl", &salient_image_url))
    snippet->set_salient_image_url(GURL(salient_image_url));
  int creation_timestamp;
  if (dict.GetInteger("creationTimestampSec", &creation_timestamp)) {
    snippet->set_publish_date(base::Time::UnixEpoch() +
                              base::TimeDelta::FromSeconds(creation_timestamp));
  }
  // TODO: Dates in json?

  return snippet;
}

}  // namespace ntp_snippets
