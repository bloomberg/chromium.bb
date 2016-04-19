// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippet.h"

#include "base/strings/string_number_conversions.h"
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
const char kSourceCorpusInfo[] = "sourceCorpusInfo";
const char kAmpUrl[] = "ampUrl";

}  // namespace

namespace ntp_snippets {

NTPSnippet::NTPSnippet(const GURL& url) : url_(url) {
  DCHECK(url_.is_valid());
}

NTPSnippet::~NTPSnippet() {}

// static
std::unique_ptr<NTPSnippet> NTPSnippet::CreateFromDictionary(
    const base::DictionaryValue& dict) {
  // Need at least the url.
  std::string url_str;
  if (!dict.GetString("url", &url_str))
    return nullptr;
  GURL url(url_str);
  if (!url.is_valid())
    return nullptr;

  std::unique_ptr<NTPSnippet> snippet(new NTPSnippet(url));

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
  // The creation and expiry timestamps are uint64s which are stored as strings.
  std::string creation_timestamp_str;
  if (dict.GetString(kPublishDate, &creation_timestamp_str))
    snippet->set_publish_date(TimeFromJsonString(creation_timestamp_str));
  std::string expiry_timestamp_str;
  if (dict.GetString(kExpiryDate, &expiry_timestamp_str))
    snippet->set_expiry_date(TimeFromJsonString(expiry_timestamp_str));

  const base::ListValue* corpus_infos_list = nullptr;
  if (dict.GetList(kSourceCorpusInfo, &corpus_infos_list)) {
    for (base::Value* value : *corpus_infos_list) {
      const base::DictionaryValue* dict_value = nullptr;
      if (value->GetAsDictionary(&dict_value)) {
        std::string amp_url;
        if (dict_value->GetString(kAmpUrl, &amp_url)) {
          snippet->set_amp_url(GURL(amp_url));
          break;
        }
      }
    }
  }

  return snippet;
}

std::unique_ptr<base::DictionaryValue> NTPSnippet::ToDictionary() const {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);

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
  if (!publish_date_.is_null())
    dict->SetString(kPublishDate, TimeToJsonString(publish_date_));
  if (!expiry_date_.is_null())
    dict->SetString(kExpiryDate, TimeToJsonString(expiry_date_));
  if (amp_url_.is_valid()) {
    std::unique_ptr<base::ListValue> corpus_infos_list(new base::ListValue);
    std::unique_ptr<base::DictionaryValue> corpus_info_dict(
        new base::DictionaryValue);
    corpus_info_dict->SetString(kAmpUrl, amp_url_.spec());
    corpus_infos_list->Set(0, std::move(corpus_info_dict));
    dict->Set(kSourceCorpusInfo, std::move(corpus_infos_list));
  }
  return dict;
}

// static
base::Time NTPSnippet::TimeFromJsonString(const std::string& timestamp_str) {
  int64_t timestamp;
  if (!base::StringToInt64(timestamp_str, &timestamp)) {
    // Even if there's an error in the conversion, some garbage data may still
    // be written to the output var, so reset it.
    timestamp = 0;
  }
  return base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(timestamp);
}

// static
std::string NTPSnippet::TimeToJsonString(const base::Time& time) {
  return base::Int64ToString((time - base::Time::UnixEpoch()).InSeconds());
}

}  // namespace ntp_snippets
