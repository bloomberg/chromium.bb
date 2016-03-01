// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippet.h"

#include "base/values.h"

namespace {

const char kUrl[] = "url";
const char kSiteTitle[] = "site_title";
const char kTitle[] = "title";
const char kFaviconUrl[] = "favicon_url";
const char kSalientImageUrl[] = "thumbnailUrl";
const char kSnippet[] = "snippet";
const char kPublishDate[] = "creationTimestampSec";
const char kExpiryDate[] = "expiryTimestampSec";

}  // namespace

namespace ntp_snippets {

NTPSnippet::NTPSnippet(const GURL& url) : url_(url) {
  DCHECK(url_.is_valid());
}

NTPSnippet::~NTPSnippet() {}

// static
scoped_ptr<NTPSnippet> NTPSnippet::CreateFromDictionary(
    const base::DictionaryValue& dict) {
  // Need at least the url.
  std::string url_str;
  if (!dict.GetString("url", &url_str))
    return nullptr;
  GURL url(url_str);
  if (!url.is_valid())
    return nullptr;

  scoped_ptr<NTPSnippet> snippet(new NTPSnippet(url));

  std::string site_title;
  if (dict.GetString(kSiteTitle, &site_title))
    snippet->set_site_title(site_title);
  std::string title;
  if (dict.GetString(kTitle, &title))
    snippet->set_title(title);
  std::string favicon_url;
  if (dict.GetString(kFaviconUrl, &favicon_url))
    snippet->set_favicon_url(GURL(favicon_url));
  std::string salient_image_url;
  if (dict.GetString(kSalientImageUrl, &salient_image_url))
    snippet->set_salient_image_url(GURL(salient_image_url));
  std::string snippet_str;
  if (dict.GetString(kSnippet, &snippet_str))
    snippet->set_snippet(snippet_str);
  int creation_timestamp;
  if (dict.GetInteger(kPublishDate, &creation_timestamp)) {
    snippet->set_publish_date(base::Time::UnixEpoch() +
                              base::TimeDelta::FromSeconds(creation_timestamp));
  }
  int expiry_timestamp;
  if (dict.GetInteger(kExpiryDate, &expiry_timestamp)) {
    snippet->set_expiry_date(base::Time::UnixEpoch() +
                             base::TimeDelta::FromSeconds(expiry_timestamp));
  }

  return snippet;
}

scoped_ptr<base::DictionaryValue> NTPSnippet::ToDictionary() const {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);

  dict->SetString(kUrl, url_.spec());
  if (!site_title_.empty())
    dict->SetString(kSiteTitle, site_title_);
  if (!title_.empty())
    dict->SetString(kTitle, title_);
  if (favicon_url_.is_valid())
    dict->SetString(kFaviconUrl, favicon_url_.spec());
  if (salient_image_url_.is_valid())
    dict->SetString(kSalientImageUrl, salient_image_url_.spec());
  if (!snippet_.empty())
    dict->SetString(kSnippet, snippet_);
  if (!publish_date_.is_null()) {
    dict->SetInteger(kPublishDate,
                     (publish_date_ - base::Time::UnixEpoch()).InSeconds());
  }
  if (!expiry_date_.is_null()) {
    dict->SetInteger(kExpiryDate,
                     (expiry_date_ - base::Time::UnixEpoch()).InSeconds());
  }

  return dict;
}

}  // namespace ntp_snippets
