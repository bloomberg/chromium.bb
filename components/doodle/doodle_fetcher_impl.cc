// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_fetcher_impl.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/google/core/browser/google_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"

using net::URLFetcher;

namespace doodle {

namespace {

const double kMaxTimeToLiveMS = 30.0 * 24 * 60 * 60 * 1000;  // 30 days

const char kDoodleConfigPath[] = "/async/ddljson";

std::string StripSafetyPreamble(const std::string& json) {
  // The response may start with )]}'. Ignore this.
  const char kResponsePreamble[] = ")]}'";

  base::StringPiece json_sp(json);
  if (json_sp.starts_with(kResponsePreamble)) {
    json_sp.remove_prefix(strlen(kResponsePreamble));
  }

  return json_sp.as_string();
}

DoodleType ParseDoodleType(const base::DictionaryValue& ddljson) {
  std::string type_str;
  ddljson.GetString("doodle_type", &type_str);
  if (type_str == "SIMPLE") {
    return DoodleType::SIMPLE;
  }
  if (type_str == "RANDOM") {
    return DoodleType::RANDOM;
  }
  if (type_str == "VIDEO") {
    return DoodleType::VIDEO;
  }
  if (type_str == "INTERACTIVE") {
    return DoodleType::INTERACTIVE;
  }
  if (type_str == "INLINE_INTERACTIVE") {
    return DoodleType::INLINE_INTERACTIVE;
  }
  if (type_str == "SLIDESHOW") {
    return DoodleType::SLIDESHOW;
  }
  return DoodleType::UNKNOWN;
}

}  // namespace

DoodleImage::DoodleImage()
    : height(0), width(0), is_animated_gif(false), is_cta(false) {}
DoodleImage::~DoodleImage() = default;

DoodleConfig::DoodleConfig() : doodle_type(DoodleType::UNKNOWN) {}
DoodleConfig::DoodleConfig(const DoodleConfig& config) = default;
DoodleConfig::~DoodleConfig() = default;

DoodleFetcherImpl::DoodleFetcherImpl(
    scoped_refptr<net::URLRequestContextGetter> download_context,
    GoogleURLTracker* google_url_tracker,
    const ParseJSONCallback& json_parsing_callback)
    : download_context_(download_context),
      json_parsing_callback_(json_parsing_callback),
      google_url_tracker_(google_url_tracker),
      weak_ptr_factory_(this) {
  DCHECK(google_url_tracker_);
}

DoodleFetcherImpl::~DoodleFetcherImpl() = default;

void DoodleFetcherImpl::FetchDoodle(FinishedCallback callback) {
  if (IsFetchInProgress()) {
    callbacks_.push_back(std::move(callback));
    return;  // The callback will be called for the existing request's results.
  }
  DCHECK(!fetcher_.get());
  callbacks_.push_back(std::move(callback));
  fetcher_ = URLFetcher::Create(GetGoogleBaseUrl().Resolve(kDoodleConfigPath),
                                URLFetcher::GET, this);
  fetcher_->SetRequestContext(download_context_.get());
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES |
                         net::LOAD_DO_NOT_SEND_AUTH_DATA);
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(1);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher_.get(), data_use_measurement::DataUseUserData::DOODLE);
  fetcher_->Start();
}

void DoodleFetcherImpl::OnURLFetchComplete(const URLFetcher* source) {
  DCHECK_EQ(fetcher_.get(), source);
  std::unique_ptr<net::URLFetcher> free_fetcher = std::move(fetcher_);

  std::string json_string;
  if (!(source->GetStatus().is_success() &&
        source->GetResponseCode() == net::HTTP_OK &&
        source->GetResponseAsString(&json_string))) {
    RespondToAllCallbacks(DoodleState::DOWNLOAD_ERROR, base::nullopt);
    return;
  }

  json_parsing_callback_.Run(StripSafetyPreamble(std::move(json_string)),
                             base::Bind(&DoodleFetcherImpl::OnJsonParsed,
                                        weak_ptr_factory_.GetWeakPtr()),
                             base::Bind(&DoodleFetcherImpl::OnJsonParseFailed,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void DoodleFetcherImpl::OnJsonParsed(std::unique_ptr<base::Value> json) {
  std::unique_ptr<base::DictionaryValue> config =
      base::DictionaryValue::From(std::move(json));
  if (!config.get()) {
    DLOG(WARNING) << "Doodle JSON is not valid.";
    RespondToAllCallbacks(DoodleState::PARSING_ERROR, base::nullopt);
    return;
  }

  const base::DictionaryValue* ddljson = nullptr;
  if (!config->GetDictionary("ddljson", &ddljson)) {
    DLOG(WARNING) << "Doodle JSON response did not contain 'ddljson' key.";
    RespondToAllCallbacks(DoodleState::PARSING_ERROR, base::nullopt);
    return;
  }

  base::Optional<DoodleConfig> doodle = ParseDoodle(*ddljson);
  if (!doodle.has_value()) {
    RespondToAllCallbacks(DoodleState::NO_DOODLE, base::nullopt);
    return;
  }

  RespondToAllCallbacks(DoodleState::AVAILABLE, std::move(doodle));
}

void DoodleFetcherImpl::OnJsonParseFailed(const std::string& error_message) {
  DLOG(WARNING) << "JSON parsing failed: " << error_message;
  RespondToAllCallbacks(DoodleState::PARSING_ERROR, base::nullopt);
}

base::Optional<DoodleConfig> DoodleFetcherImpl::ParseDoodle(
    const base::DictionaryValue& ddljson) const {
  DoodleConfig doodle;
  // The |large_image| field is required (it's the "default" representation for
  // the doodle).
  if (!ParseImage(ddljson, "large_image", &doodle.large_image)) {
    return base::nullopt;
  }
  ParseImage(ddljson, "transparent_large_image",
             &doodle.transparent_large_image);
  ParseImage(ddljson, "large_cta_image", &doodle.large_cta_image);
  ParseBaseInformation(ddljson, &doodle);
  return doodle;
}

bool DoodleFetcherImpl::ParseImage(const base::DictionaryValue& image_parent,
                                   const std::string& image_name,
                                   DoodleImage* image) const {
  DCHECK(image);
  const base::DictionaryValue* image_dict = nullptr;
  if (!image_parent.GetDictionary(image_name, &image_dict)) {
    return false;
  }
  image->url = ParseRelativeUrl(*image_dict, "url");
  if (!image->url.is_valid()) {
    DLOG(WARNING) << "Image URL for \"" << image_name << "\" is invalid.";
    return false;
  }
  image_dict->GetInteger("height", &image->height);
  image_dict->GetInteger("width", &image->width);
  image_dict->GetBoolean("is_animated_gif", &image->is_animated_gif);
  image_dict->GetBoolean("is_cta", &image->is_cta);
  return true;
}

void DoodleFetcherImpl::ParseBaseInformation(
    const base::DictionaryValue& ddljson,
    DoodleConfig* config) const {
  config->search_url = ParseRelativeUrl(ddljson, "search_url");
  config->target_url = ParseRelativeUrl(ddljson, "target_url");
  config->fullpage_interactive_url =
      ParseRelativeUrl(ddljson, "fullpage_interactive_url");

  config->doodle_type = ParseDoodleType(ddljson);
  ddljson.GetString("alt_text", &config->alt_text);
  ddljson.GetString("interactive_html", &config->interactive_html);

  // The JSON doesn't guarantee the number to fit into an int.
  double ttl = 0;  // Expires immediately if the parameter is missing.
  if (!ddljson.GetDouble("time_to_live_ms", &ttl) || ttl < 0) {
    DLOG(WARNING) << "No valid Doodle TTL present in ddljson!";
    ttl = 0;
  }
  // TODO(treib,fhorschig): Move this logic into the service.
  if (ttl > kMaxTimeToLiveMS) {
    ttl = kMaxTimeToLiveMS;
    DLOG(WARNING) << "Clamping Doodle TTL to 30 days!";
  }
  config->time_to_live = base::TimeDelta::FromMillisecondsD(ttl);
}

GURL DoodleFetcherImpl::ParseRelativeUrl(
    const base::DictionaryValue& dict_value,
    const std::string& key) const {
  std::string str_url;
  dict_value.GetString(key, &str_url);
  if (str_url.empty()) {
    return GURL();
  }
  return GetGoogleBaseUrl().Resolve(str_url);
}

void DoodleFetcherImpl::RespondToAllCallbacks(
    DoodleState state,
    const base::Optional<DoodleConfig>& config) {
  for (auto& callback : callbacks_) {
    std::move(callback).Run(state, config);
  }
  callbacks_.clear();
}

GURL DoodleFetcherImpl::GetGoogleBaseUrl() const {
  GURL cmd_line_url = google_util::CommandLineGoogleBaseURL();
  if (cmd_line_url.is_valid()) {
    return cmd_line_url;
  }
  return google_url_tracker_->google_url();
}

}  // namespace doodle
