// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippet.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace {

const char kUrl[] = "url";
const char kTitle[] = "title";
const char kSalientImageUrl[] = "thumbnailUrl";
const char kSnippet[] = "snippet";
const char kPublishDate[] = "creationTimestampSec";
const char kExpiryDate[] = "expiryTimestampSec";
const char kSiteTitle[] = "sourceName";
const char kPublisherData[] = "publisherData";
const char kCorpusId[] = "corpusId";
const char kSourceCorpusInfo[] = "sourceCorpusInfo";
const char kAmpUrl[] = "ampUrl";

}  // namespace

namespace ntp_snippets {

NTPSnippet::NTPSnippet(const GURL& url) : url_(url), best_source_index_(0) {
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

  std::string title;
  if (dict.GetString(kTitle, &title))
    snippet->set_title(title);
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
  if (!dict.GetList(kSourceCorpusInfo, &corpus_infos_list)) {
    DLOG(WARNING) << "No sources found for article " << title;
    return nullptr;
  }

  for (base::Value* value : *corpus_infos_list) {
    const base::DictionaryValue* dict_value = nullptr;
    if (!value->GetAsDictionary(&dict_value)) {
      DLOG(WARNING) << "Invalid source info for article " << url_str;
      continue;
    }

    std::string corpus_id_str;
    GURL corpus_id;
    if (dict_value->GetString(kCorpusId, &corpus_id_str))
      corpus_id = GURL(corpus_id_str);

    if (!corpus_id.is_valid()) {
      // We must at least have a valid source URL.
      DLOG(WARNING) << "Invalid article url " << corpus_id_str;
      continue;
    }

    const base::DictionaryValue* publisher_data = nullptr;
    std::string site_title;
    if (dict_value->GetDictionary(kPublisherData, &publisher_data)) {
      if (!publisher_data->GetString(kSiteTitle, &site_title)) {
        // It's possible but not desirable to have no publisher data.
        DLOG(WARNING) << "No publisher name for article " << corpus_id.spec();
      }
    } else {
      DLOG(WARNING) << "No publisher data for article " << corpus_id.spec();
    }

    std::string amp_url_str;
    GURL amp_url;
    // Expected to not have AMP url sometimes.
    if (dict_value->GetString(kAmpUrl, &amp_url_str)) {
      amp_url = GURL(amp_url_str);
      DLOG_IF(WARNING, !amp_url.is_valid()) << "Invalid AMP url "
                                            << amp_url_str;
    }
    SnippetSource source(corpus_id, site_title,
                         amp_url.is_valid() ? amp_url : GURL());
    snippet->add_source(source);
  }
  // The previous url we have saved can be one of several sources for the
  // article. For example, the same article can be hosted by nytimes.com,
  // cnn.com, etc. We need to parse the list of sources for this article and
  // find the best match. In order of preference:
  //  1) A source that has url, publisher name, AMP url
  //  2) A source that has url, publisher name
  //  3) A source that has url and AMP url, or url only (since we won't show
  //  the snippet to users if the article does not have a publisher name, it
  //  doesn't matter whether the snippet has the AMP url or not)
  size_t best_source_index = 0;
  for (size_t i = 0; i < snippet->sources_.size(); ++i) {
    const SnippetSource& source = snippet->sources_[i];
    if (!source.publisher_name.empty()) {
      best_source_index = i;
      if (!source.amp_url.is_empty()) {
        // This is the best possible source, stop looking.
        break;
      }
    }
  }
  snippet->set_source_index(best_source_index);

  if (snippet->sources_.empty()) {
    DLOG(WARNING) << "No sources found for article " << url_str;
    return nullptr;
  }

  return snippet;
}

std::unique_ptr<base::DictionaryValue> NTPSnippet::ToDictionary() const {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);

  dict->SetString(kUrl, url_.spec());
  if (!title_.empty())
    dict->SetString(kTitle, title_);
  if (salient_image_url_.is_valid())
    dict->SetString(kSalientImageUrl, salient_image_url_.spec());
  if (!snippet_.empty())
    dict->SetString(kSnippet, snippet_);
  if (!publish_date_.is_null())
    dict->SetString(kPublishDate, TimeToJsonString(publish_date_));
  if (!expiry_date_.is_null())
    dict->SetString(kExpiryDate, TimeToJsonString(expiry_date_));

  std::unique_ptr<base::ListValue> corpus_infos_list(new base::ListValue);
  for (const SnippetSource& source : sources_) {
    std::unique_ptr<base::DictionaryValue> corpus_info_dict(
        new base::DictionaryValue);

    corpus_info_dict->SetString(kCorpusId, source.url.spec());
    if (!source.amp_url.is_empty())
      corpus_info_dict->SetString(kAmpUrl, source.amp_url.spec());
    if (!source.publisher_name.empty())
      corpus_info_dict->SetString(
          base::StringPrintf("%s.%s", kPublisherData, kSiteTitle),
          source.publisher_name);

    corpus_infos_list->Append(std::move(corpus_info_dict));
  }

  dict->Set(kSourceCorpusInfo, std::move(corpus_infos_list));

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
