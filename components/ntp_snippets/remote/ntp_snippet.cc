// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/ntp_snippet.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"

namespace {

struct SnippetSource {
  SnippetSource(const GURL& url,
                const std::string& publisher_name,
                const GURL& amp_url)
      : url(url), publisher_name(publisher_name), amp_url(amp_url) {}
  GURL url;
  std::string publisher_name;
  GURL amp_url;
};

const SnippetSource& FindBestSource(const std::vector<SnippetSource>& sources) {
  // The same article can be hosted by multiple sources, e.g. nytimes.com,
  // cnn.com, etc. We need to parse the list of sources for this article and
  // find the best match. In order of preference:
  //  1) A source that has URL, publisher name, AMP URL
  //  2) A source that has URL, publisher name
  //  3) A source that has URL and AMP URL, or URL only (since we won't show
  //  the snippet to users if the article does not have a publisher name, it
  //  doesn't matter whether the snippet has the AMP URL or not)
  int best_source_index = 0;
  for (size_t i = 0; i < sources.size(); ++i) {
    const SnippetSource& source = sources[i];
    if (!source.publisher_name.empty()) {
      best_source_index = i;
      if (!source.amp_url.is_empty()) {
        // This is the best possible source, stop looking.
        break;
      }
    }
  }
  return sources[best_source_index];
}

// dict.Get() specialization for base::Time values
bool GetTimeValue(const base::DictionaryValue& dict,
                  const std::string& key,
                  base::Time* time) {
  std::string time_value;
  return dict.GetString(key, &time_value) &&
         base::Time::FromString(time_value.c_str(), time);
}

// dict.Get() specialization for GURL values
bool GetURLValue(const base::DictionaryValue& dict,
                 const std::string& key,
                 GURL* url) {
  std::string spec;
  if (!dict.GetString(key, &spec)) {
    return false;
  }
  *url = GURL(spec);
  return url->is_valid();
}

}  // namespace

namespace ntp_snippets {

const int kArticlesRemoteId = 1;
static_assert(
    static_cast<int>(KnownCategories::ARTICLES) -
            static_cast<int>(KnownCategories::REMOTE_CATEGORIES_OFFSET) ==
        kArticlesRemoteId,
    "kArticlesRemoteId has a wrong value?!");

const int kChromeReaderDefaultExpiryTimeMins = 3 * 24 * 60;

NTPSnippet::NTPSnippet(const std::string& id, int remote_category_id)
    : ids_(1, id),
      score_(0),
      is_dismissed_(false),
      remote_category_id_(remote_category_id) {}

NTPSnippet::~NTPSnippet() = default;

// static
std::unique_ptr<NTPSnippet> NTPSnippet::CreateFromChromeReaderDictionary(
    const base::DictionaryValue& dict) {
  const base::DictionaryValue* content = nullptr;
  if (!dict.GetDictionary("contentInfo", &content)) {
    return nullptr;
  }

  // Need at least the id.
  std::string id;
  if (!content->GetString("url", &id) || id.empty()) {
    return nullptr;
  }

  std::unique_ptr<NTPSnippet> snippet(new NTPSnippet(id, kArticlesRemoteId));

  std::string title;
  if (content->GetString("title", &title)) {
    snippet->title_ = title;
  }
  std::string salient_image_url;
  if (content->GetString("thumbnailUrl", &salient_image_url)) {
    snippet->salient_image_url_ = GURL(salient_image_url);
  }
  std::string snippet_str;
  if (content->GetString("snippet", &snippet_str)) {
    snippet->snippet_ = snippet_str;
  }
  // The creation and expiry timestamps are uint64s which are stored as strings.
  std::string creation_timestamp_str;
  if (content->GetString("creationTimestampSec", &creation_timestamp_str)) {
    snippet->publish_date_ = TimeFromJsonString(creation_timestamp_str);
  }
  std::string expiry_timestamp_str;
  if (content->GetString("expiryTimestampSec", &expiry_timestamp_str)) {
    snippet->expiry_date_ = TimeFromJsonString(expiry_timestamp_str);
  }

  // If publish and/or expiry date are missing, fill in reasonable defaults.
  if (snippet->publish_date_.is_null()) {
    snippet->publish_date_ = base::Time::Now();
  }
  if (snippet->expiry_date_.is_null()) {
    snippet->expiry_date_ =
        snippet->publish_date() +
        base::TimeDelta::FromMinutes(kChromeReaderDefaultExpiryTimeMins);
  }

  const base::ListValue* corpus_infos_list = nullptr;
  if (!content->GetList("sourceCorpusInfo", &corpus_infos_list)) {
    DLOG(WARNING) << "No sources found for article " << title;
    return nullptr;
  }

  std::vector<std::string> additional_ids;
  std::vector<SnippetSource> sources;
  for (const auto& value : *corpus_infos_list) {
    const base::DictionaryValue* dict_value = nullptr;
    if (!value->GetAsDictionary(&dict_value)) {
      DLOG(WARNING) << "Invalid source info for article " << id;
      continue;
    }

    std::string corpus_id_str;
    GURL corpus_id;
    if (dict_value->GetString("corpusId", &corpus_id_str)) {
      corpus_id = GURL(corpus_id_str);
    }

    if (!corpus_id.is_valid()) {
      // We must at least have a valid source URL.
      DLOG(WARNING) << "Invalid article url " << corpus_id_str;
      continue;
    }
    const base::DictionaryValue* publisher_data = nullptr;
    std::string site_title;
    if (dict_value->GetDictionary("publisherData", &publisher_data)) {
      if (!publisher_data->GetString("sourceName", &site_title)) {
        // It's possible but not desirable to have no publisher data.
        DLOG(WARNING) << "No publisher name for article " << corpus_id_str;
      }
    } else {
      DLOG(WARNING) << "No publisher data for article " << corpus_id_str;
    }

    std::string amp_url_str;
    GURL amp_url;
    // Expected to not have AMP url sometimes.
    if (dict_value->GetString("ampUrl", &amp_url_str)) {
      amp_url = GURL(amp_url_str);
      DLOG_IF(WARNING, !amp_url.is_valid()) << "Invalid AMP url "
                                            << amp_url_str;
    }
    sources.emplace_back(corpus_id, site_title,
                         amp_url.is_valid() ? amp_url : GURL());
    // We use the raw string so that we can compare it against other primary
    // IDs. Parsing the ID as a URL might add a trailing slash (and we don't do
    // this for the primary ID).
    additional_ids.push_back(corpus_id_str);
  }
  snippet->AddIDs(additional_ids);

  if (sources.empty()) {
    DLOG(WARNING) << "No sources found for article " << id;
    return nullptr;
  }
  const SnippetSource& source = FindBestSource(sources);
  snippet->url_ = source.url;
  snippet->publisher_name_ = source.publisher_name;
  snippet->amp_url_ = source.amp_url;

  double score;
  if (dict.GetDouble("score", &score)) {
    snippet->score_ = score;
  }

  return snippet;
}

// static
std::unique_ptr<NTPSnippet> NTPSnippet::CreateFromContentSuggestionsDictionary(
    const base::DictionaryValue& dict,
    int remote_category_id) {
  const base::ListValue* ids;
  if (!dict.GetList("ids", &ids)) {
    return nullptr;
  }
  std::vector<std::string> parsed_ids;
  for (const auto& value : *ids) {
    std::string id;
    if (!value->GetAsString(&id)) {
      return nullptr;
    }
    parsed_ids.push_back(id);
  }

  if (parsed_ids.empty()) {
    return nullptr;
  }
  auto snippet =
      base::MakeUnique<NTPSnippet>(parsed_ids.front(), remote_category_id);
  parsed_ids.erase(parsed_ids.begin(), parsed_ids.begin() + 1);
  snippet->AddIDs(parsed_ids);

  if (!(dict.GetString("title", &snippet->title_) &&
        dict.GetString("snippet", &snippet->snippet_) &&
        GetTimeValue(dict, "creationTime", &snippet->publish_date_) &&
        GetTimeValue(dict, "expirationTime", &snippet->expiry_date_) &&
        GetURLValue(dict, "imageUrl", &snippet->salient_image_url_) &&
        dict.GetString("attribution", &snippet->publisher_name_) &&
        GetURLValue(dict, "fullPageUrl", &snippet->url_))) {
    return nullptr;
  }
  GetURLValue(dict, "ampUrl", &snippet->amp_url_);  // May fail; OK.
  // TODO(sfiera): also favicon URL.

  double score;
  if (dict.GetDouble("score", &score)) {
    snippet->score_ = score;
  }

  return snippet;
}

// static
std::unique_ptr<NTPSnippet> NTPSnippet::CreateFromProto(
    const SnippetProto& proto) {
  // Need at least the id.
  if (proto.ids_size() == 0 || proto.ids(0).empty()) {
    return nullptr;
  }

  int remote_category_id = proto.has_remote_category_id()
                               ? proto.remote_category_id()
                               : kArticlesRemoteId;

  auto snippet = base::MakeUnique<NTPSnippet>(proto.ids(0), remote_category_id);
  snippet->AddIDs(
      std::vector<std::string>(proto.ids().begin() + 1, proto.ids().end()));

  snippet->title_ = proto.title();
  snippet->snippet_ = proto.snippet();
  snippet->salient_image_url_ = GURL(proto.salient_image_url());
  snippet->publish_date_ = base::Time::FromInternalValue(proto.publish_date());
  snippet->expiry_date_ = base::Time::FromInternalValue(proto.expiry_date());
  snippet->score_ = proto.score();
  snippet->is_dismissed_ = proto.dismissed();

  std::vector<SnippetSource> sources;
  for (int i = 0; i < proto.sources_size(); ++i) {
    const SnippetSourceProto& source_proto = proto.sources(i);
    GURL url(source_proto.url());
    if (!url.is_valid()) {
      // We must at least have a valid source URL.
      DLOG(WARNING) << "Invalid article url " << source_proto.url();
      continue;
    }
    GURL amp_url;
    if (source_proto.has_amp_url()) {
      amp_url = GURL(source_proto.amp_url());
      DLOG_IF(WARNING, !amp_url.is_valid()) << "Invalid AMP URL "
                                            << source_proto.amp_url();
    }

    sources.emplace_back(url, source_proto.publisher_name(), amp_url);
  }

  if (sources.empty()) {
    DLOG(WARNING) << "No sources found for article " << snippet->id();
    return nullptr;
  }
  const SnippetSource& source = FindBestSource(sources);
  snippet->url_ = source.url;
  snippet->publisher_name_ = source.publisher_name;
  snippet->amp_url_ = source.amp_url;

  return snippet;
}

// static
std::unique_ptr<NTPSnippet> NTPSnippet::CreateForTesting(
    const std::string& id,
    int remote_category_id,
    const GURL& url,
    const std::string& publisher_name,
    const GURL& amp_url) {
  auto snippet = base::MakeUnique<NTPSnippet>(id, remote_category_id);
  snippet->url_ = url;
  snippet->publisher_name_ = publisher_name;
  snippet->amp_url_ = amp_url;

  return snippet;
}

SnippetProto NTPSnippet::ToProto() const {
  SnippetProto result;
  for (const std::string& id : ids_) {
    result.add_ids(id);
  }
  if (!title_.empty()) {
    result.set_title(title_);
  }
  if (!snippet_.empty()) {
    result.set_snippet(snippet_);
  }
  if (salient_image_url_.is_valid()) {
    result.set_salient_image_url(salient_image_url_.spec());
  }
  if (!publish_date_.is_null()) {
    result.set_publish_date(publish_date_.ToInternalValue());
  }
  if (!expiry_date_.is_null()) {
    result.set_expiry_date(expiry_date_.ToInternalValue());
  }
  result.set_score(score_);
  result.set_dismissed(is_dismissed_);
  result.set_remote_category_id(remote_category_id_);

  SnippetSourceProto* source_proto = result.add_sources();
  source_proto->set_url(url_.spec());
  if (!publisher_name_.empty()) {
    source_proto->set_publisher_name(publisher_name_);
  }
  if (amp_url_.is_valid()) {
    source_proto->set_amp_url(amp_url_.spec());
  }

  return result;
}

void NTPSnippet::AddIDs(const std::vector<std::string>& ids) {
  ids_.insert(ids_.end(), ids.begin(), ids.end());
}

// static
base::Time NTPSnippet::TimeFromJsonString(const std::string& timestamp_str) {
  int64_t timestamp;
  if (!base::StringToInt64(timestamp_str, &timestamp)) {
    // Even if there's an error in the conversion, some garbage data may still
    // be written to the output var, so reset it.
    DLOG(WARNING) << "Invalid json timestamp: " << timestamp_str;
    timestamp = 0;
  }
  return base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(timestamp);
}

// static
std::string NTPSnippet::TimeToJsonString(const base::Time& time) {
  return base::Int64ToString((time - base::Time::UnixEpoch()).InSeconds());
}

}  // namespace ntp_snippets
