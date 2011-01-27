// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_download.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/autofill_xml_parser.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "net/http/http_response_headers.h"

#define DISABLED_REQUEST_URL "http://disabled"

#if defined(GOOGLE_CHROME_BUILD)
#define AUTO_FILL_QUERY_SERVER_REQUEST_URL \
    "http://toolbarqueries.clients.google.com:80/tbproxy/af/query"
#define AUTO_FILL_UPLOAD_SERVER_REQUEST_URL \
    "http://toolbarqueries.clients.google.com:80/tbproxy/af/upload"
#define AUTO_FILL_QUERY_SERVER_NAME_START_IN_HEADER "GFE/"
#else
#define AUTO_FILL_QUERY_SERVER_REQUEST_URL DISABLED_REQUEST_URL
#define AUTO_FILL_UPLOAD_SERVER_REQUEST_URL DISABLED_REQUEST_URL
#define AUTO_FILL_QUERY_SERVER_NAME_START_IN_HEADER "SOMESERVER/"
#endif

namespace {
const size_t kMaxFormCacheSize = 16;
};

struct AutoFillDownloadManager::FormRequestData {
  std::vector<std::string> form_signatures;
  AutoFillRequestType request_type;
};

AutoFillDownloadManager::AutoFillDownloadManager(Profile* profile)
    : profile_(profile),
      observer_(NULL),
      max_form_cache_size_(kMaxFormCacheSize),
      next_query_request_(base::Time::Now()),
      next_upload_request_(base::Time::Now()),
      positive_upload_rate_(0),
      negative_upload_rate_(0),
      fetcher_id_for_unittest_(0),
      is_testing_(false) {
  // |profile_| could be NULL in some unit-tests.
  if (profile_) {
    PrefService* preferences = profile_->GetPrefs();
    positive_upload_rate_ =
        preferences->GetReal(prefs::kAutoFillPositiveUploadRate);
    negative_upload_rate_ =
        preferences->GetReal(prefs::kAutoFillNegativeUploadRate);
  }
}

AutoFillDownloadManager::~AutoFillDownloadManager() {
  STLDeleteContainerPairFirstPointers(url_fetchers_.begin(),
                                      url_fetchers_.end());
}

void AutoFillDownloadManager::SetObserver(
    AutoFillDownloadManager::Observer *observer) {
  if (observer) {
    DCHECK(!observer_);
    observer_ = observer;
  } else {
    observer_ = NULL;
  }
}

bool AutoFillDownloadManager::StartQueryRequest(
    const ScopedVector<FormStructure>& forms,
    const AutoFillMetrics& metric_logger) {
  if (next_query_request_ > base::Time::Now()) {
    // We are in back-off mode: do not do the request.
    return false;
  }
  std::string form_xml;
  FormRequestData request_data;
  if (!FormStructure::EncodeQueryRequest(forms, &request_data.form_signatures,
                                         &form_xml)) {
    return false;
  }

  request_data.request_type = AutoFillDownloadManager::REQUEST_QUERY;
  metric_logger.Log(AutoFillMetrics::QUERY_SENT);

  std::string query_data;
  if (CheckCacheForQueryRequest(request_data.form_signatures, &query_data)) {
    VLOG(1) << "AutoFillDownloadManager: query request has been retrieved from"
            << "the cache";
    if (observer_)
      observer_->OnLoadedAutoFillHeuristics(query_data);
    return true;
  }

  return StartRequest(form_xml, request_data);
}

bool AutoFillDownloadManager::StartUploadRequest(
    const FormStructure& form, bool form_was_matched) {
  if (next_upload_request_ > base::Time::Now()) {
    // We are in back-off mode: do not do the request.
    return false;
  }

  // Check if we need to upload form.
  double upload_rate = form_was_matched ? GetPositiveUploadRate() :
                                          GetNegativeUploadRate();
  if (base::RandDouble() > upload_rate) {
    VLOG(1) << "AutoFillDownloadManager: Upload request is ignored";
    // If we ever need notification that upload was skipped, add it here.
    return false;
  }
  std::string form_xml;
  if (!form.EncodeUploadRequest(form_was_matched, &form_xml))
    return false;

  FormRequestData request_data;
  request_data.form_signatures.push_back(form.FormSignature());
  request_data.request_type = AutoFillDownloadManager::REQUEST_UPLOAD;

  return StartRequest(form_xml, request_data);
}

bool AutoFillDownloadManager::CancelRequest(
    const std::string& form_signature,
    AutoFillDownloadManager::AutoFillRequestType request_type) {
  for (std::map<URLFetcher *, FormRequestData>::iterator it =
       url_fetchers_.begin();
       it != url_fetchers_.end();
       ++it) {
    if (std::find(it->second.form_signatures.begin(),
        it->second.form_signatures.end(), form_signature) !=
        it->second.form_signatures.end() &&
        it->second.request_type == request_type) {
      delete it->first;
      url_fetchers_.erase(it);
      return true;
    }
  }
  return false;
}

double AutoFillDownloadManager::GetPositiveUploadRate() const {
  return positive_upload_rate_;
}

double AutoFillDownloadManager::GetNegativeUploadRate() const {
  return negative_upload_rate_;
}

void AutoFillDownloadManager::SetPositiveUploadRate(double rate) {
  if (rate == positive_upload_rate_)
    return;
  positive_upload_rate_ = rate;
  DCHECK_GE(rate, 0.0);
  DCHECK_LE(rate, 1.0);
  DCHECK(profile_);
  PrefService* preferences = profile_->GetPrefs();
  preferences->SetReal(prefs::kAutoFillPositiveUploadRate, rate);
}

void AutoFillDownloadManager::SetNegativeUploadRate(double rate) {
  if (rate == negative_upload_rate_)
    return;
  negative_upload_rate_ = rate;
  DCHECK_GE(rate, 0.0);
  DCHECK_LE(rate, 1.0);
  DCHECK(profile_);
  PrefService* preferences = profile_->GetPrefs();
  preferences->SetReal(prefs::kAutoFillNegativeUploadRate, rate);
}

bool AutoFillDownloadManager::StartRequest(
    const std::string& form_xml,
    const FormRequestData& request_data) {
  std::string request_url;
  if (request_data.request_type == AutoFillDownloadManager::REQUEST_QUERY)
    request_url = AUTO_FILL_QUERY_SERVER_REQUEST_URL;
  else
    request_url = AUTO_FILL_UPLOAD_SERVER_REQUEST_URL;

  if (!request_url.compare(DISABLED_REQUEST_URL) && !is_testing_) {
    // We have it disabled - return true as if it succeeded, but do nothing.
    return true;
  }

  // Id is ignored for regular chrome, in unit test id's for fake fetcher
  // factory will be 0, 1, 2, ...
  URLFetcher *fetcher = URLFetcher::Create(fetcher_id_for_unittest_++,
                                           GURL(request_url),
                                           URLFetcher::POST,
                                           this);
  url_fetchers_[fetcher] = request_data;
  fetcher->set_automatically_retry_on_5xx(false);
  fetcher->set_request_context(Profile::GetDefaultRequestContext());
  fetcher->set_upload_data("text/plain", form_xml);
  fetcher->Start();
  return true;
}

void AutoFillDownloadManager::CacheQueryRequest(
    const std::vector<std::string>& forms_in_query,
    const std::string& query_data) {
  std::string signature = GetCombinedSignature(forms_in_query);
  for (QueryRequestCache::iterator it = cached_forms_.begin();
       it != cached_forms_.end(); ++it) {
    if (it->first == signature) {
      // We hit the cache, move to the first position and return.
      std::pair<std::string, std::string> data = *it;
      cached_forms_.erase(it);
      cached_forms_.push_front(data);
      return;
    }
  }
  std::pair<std::string, std::string> data;
  data.first = signature;
  data.second = query_data;
  cached_forms_.push_front(data);
  while (cached_forms_.size() > max_form_cache_size_)
    cached_forms_.pop_back();
}

bool AutoFillDownloadManager::CheckCacheForQueryRequest(
    const std::vector<std::string>& forms_in_query,
    std::string* query_data) const {
  std::string signature = GetCombinedSignature(forms_in_query);
  for (QueryRequestCache::const_iterator it = cached_forms_.begin();
       it != cached_forms_.end(); ++it) {
    if (it->first == signature) {
      // We hit the cache, fill the data and return.
      *query_data = it->second;
      return true;
    }
  }
  return false;
}

std::string AutoFillDownloadManager::GetCombinedSignature(
    const std::vector<std::string>& forms_in_query) const {
  size_t total_size = forms_in_query.size();
  for (size_t i = 0; i < forms_in_query.size(); ++i)
    total_size += forms_in_query[i].length();
  std::string signature;

  signature.reserve(total_size);

  for (size_t i = 0; i < forms_in_query.size(); ++i) {
    if (i)
      signature.append(",");
    signature.append(forms_in_query[i]);
  }
  return signature;
}

void AutoFillDownloadManager::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  std::map<URLFetcher *, FormRequestData>::iterator it =
      url_fetchers_.find(const_cast<URLFetcher*>(source));
  if (it == url_fetchers_.end()) {
    // Looks like crash on Mac is possibly caused with callback entering here
    // with unknown fetcher when network is refreshed.
    return;
  }
  std::string type_of_request(
      it->second.request_type == AutoFillDownloadManager::REQUEST_QUERY ?
          "query" : "upload");
  const int kHttpResponseOk = 200;
  const int kHttpInternalServerError = 500;
  const int kHttpBadGateway = 502;
  const int kHttpServiceUnavailable = 503;

  CHECK(it->second.form_signatures.size());
  if (response_code != kHttpResponseOk) {
    bool back_off = false;
    std::string server_header;
    switch (response_code) {
      case kHttpBadGateway:
        if (!source->response_headers()->EnumerateHeader(NULL, "server",
                                                         &server_header) ||
            StartsWithASCII(server_header.c_str(),
                            AUTO_FILL_QUERY_SERVER_NAME_START_IN_HEADER,
                            false) != 0)
          break;
        // Bad getaway was received from AutoFill servers. Fall through to back
        // off.
      case kHttpInternalServerError:
      case kHttpServiceUnavailable:
        back_off = true;
        break;
    }

    if (back_off) {
      base::Time back_off_time(base::Time::Now() + source->backoff_delay());
      if (it->second.request_type == AutoFillDownloadManager::REQUEST_QUERY) {
        next_query_request_ = back_off_time;
      } else {
        next_upload_request_ = back_off_time;
      }
    }

    LOG(WARNING) << "AutoFillDownloadManager: " << type_of_request
                 << " request has failed with response " << response_code;
    if (observer_) {
      observer_->OnHeuristicsRequestError(it->second.form_signatures[0],
                                          it->second.request_type,
                                          response_code);
    }
  } else {
    VLOG(1) << "AutoFillDownloadManager: " << type_of_request
            << " request has succeeded";
    if (it->second.request_type == AutoFillDownloadManager::REQUEST_QUERY) {
      CacheQueryRequest(it->second.form_signatures, data);
      if (observer_)
        observer_->OnLoadedAutoFillHeuristics(data);
    } else {
      double new_positive_upload_rate = 0;
      double new_negative_upload_rate = 0;
      AutoFillUploadXmlParser parse_handler(&new_positive_upload_rate,
                                            &new_negative_upload_rate);
      buzz::XmlParser parser(&parse_handler);
      parser.Parse(data.data(), data.length(), true);
      if (parse_handler.succeeded()) {
        SetPositiveUploadRate(new_positive_upload_rate);
        SetNegativeUploadRate(new_negative_upload_rate);
      }

      if (observer_)
        observer_->OnUploadedAutoFillHeuristics(it->second.form_signatures[0]);
    }
  }
  delete it->first;
  url_fetchers_.erase(it);
}
