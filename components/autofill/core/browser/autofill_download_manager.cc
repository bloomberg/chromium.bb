// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_download_manager.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_xml_parser.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "third_party/webrtc/libjingle/xmllite/xmlparser.h"
#include "url/gurl.h"

namespace autofill {

namespace {

const char kAutofillQueryServerNameStartInHeader[] = "GFE/";
const size_t kMaxFormCacheSize = 16;

#if defined(GOOGLE_CHROME_BUILD)
const char kClientName[] = "Google Chrome";
#else
const char kClientName[] = "Chromium";
#endif  // defined(GOOGLE_CHROME_BUILD)

std::string RequestTypeToString(AutofillDownloadManager::RequestType type) {
  switch (type) {
    case AutofillDownloadManager::REQUEST_QUERY:
      return "query";
    case AutofillDownloadManager::REQUEST_UPLOAD:
      return "upload";
  }
  NOTREACHED();
  return std::string();
}

GURL GetRequestUrl(AutofillDownloadManager::RequestType request_type) {
  return GURL("https://clients1.google.com/tbproxy/af/" +
              RequestTypeToString(request_type) + "?client=" + kClientName);
}

}  // namespace

struct AutofillDownloadManager::FormRequestData {
  std::vector<std::string> form_signatures;
  RequestType request_type;
};

AutofillDownloadManager::AutofillDownloadManager(AutofillDriver* driver,
                                                 PrefService* pref_service,
                                                 Observer* observer)
    : driver_(driver),
      pref_service_(pref_service),
      observer_(observer),
      max_form_cache_size_(kMaxFormCacheSize),
      next_query_request_(base::Time::Now()),
      next_upload_request_(base::Time::Now()),
      positive_upload_rate_(0),
      negative_upload_rate_(0),
      fetcher_id_for_unittest_(0) {
  DCHECK(observer_);
  positive_upload_rate_ =
      pref_service_->GetDouble(prefs::kAutofillPositiveUploadRate);
  negative_upload_rate_ =
      pref_service_->GetDouble(prefs::kAutofillNegativeUploadRate);
}

AutofillDownloadManager::~AutofillDownloadManager() {
  STLDeleteContainerPairFirstPointers(url_fetchers_.begin(),
                                      url_fetchers_.end());
}

bool AutofillDownloadManager::StartQueryRequest(
    const std::vector<FormStructure*>& forms,
    const AutofillMetrics& metric_logger) {
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

  request_data.request_type = AutofillDownloadManager::REQUEST_QUERY;
  metric_logger.LogServerQueryMetric(AutofillMetrics::QUERY_SENT);

  std::string query_data;
  if (CheckCacheForQueryRequest(request_data.form_signatures, &query_data)) {
    DVLOG(1) << "AutofillDownloadManager: query request has been retrieved "
             << "from the cache, form signatures: "
             << GetCombinedSignature(request_data.form_signatures);
    observer_->OnLoadedServerPredictions(query_data);
    return true;
  }

  return StartRequest(form_xml, request_data);
}

bool AutofillDownloadManager::StartUploadRequest(
    const FormStructure& form,
    bool form_was_autofilled,
    const ServerFieldTypeSet& available_field_types) {
  std::string form_xml;
  if (!form.EncodeUploadRequest(available_field_types, form_was_autofilled,
                                &form_xml))
    return false;

  if (next_upload_request_ > base::Time::Now()) {
    // We are in back-off mode: do not do the request.
    DVLOG(1) << "AutofillDownloadManager: Upload request is throttled.";
    return false;
  }

  // Flip a coin to see if we should upload this form.
  double upload_rate = form_was_autofilled ? GetPositiveUploadRate() :
                                             GetNegativeUploadRate();
  if (form.upload_required() == UPLOAD_NOT_REQUIRED ||
      (form.upload_required() == USE_UPLOAD_RATES &&
       base::RandDouble() > upload_rate)) {
    DVLOG(1) << "AutofillDownloadManager: Upload request is ignored.";
    // If we ever need notification that upload was skipped, add it here.
    return false;
  }

  FormRequestData request_data;
  request_data.form_signatures.push_back(form.FormSignature());
  request_data.request_type = AutofillDownloadManager::REQUEST_UPLOAD;

  return StartRequest(form_xml, request_data);
}

double AutofillDownloadManager::GetPositiveUploadRate() const {
  return positive_upload_rate_;
}

double AutofillDownloadManager::GetNegativeUploadRate() const {
  return negative_upload_rate_;
}

void AutofillDownloadManager::SetPositiveUploadRate(double rate) {
  if (rate == positive_upload_rate_)
    return;
  positive_upload_rate_ = rate;
  DCHECK_GE(rate, 0.0);
  DCHECK_LE(rate, 1.0);
  pref_service_->SetDouble(prefs::kAutofillPositiveUploadRate, rate);
}

void AutofillDownloadManager::SetNegativeUploadRate(double rate) {
  if (rate == negative_upload_rate_)
    return;
  negative_upload_rate_ = rate;
  DCHECK_GE(rate, 0.0);
  DCHECK_LE(rate, 1.0);
  pref_service_->SetDouble(prefs::kAutofillNegativeUploadRate, rate);
}

bool AutofillDownloadManager::StartRequest(
    const std::string& form_xml,
    const FormRequestData& request_data) {
  net::URLRequestContextGetter* request_context =
      driver_->GetURLRequestContext();
  DCHECK(request_context);
  GURL request_url = GetRequestUrl(request_data.request_type);

  // Id is ignored for regular chrome, in unit test id's for fake fetcher
  // factory will be 0, 1, 2, ...
  net::URLFetcher* fetcher = net::URLFetcher::Create(
      fetcher_id_for_unittest_++, request_url, net::URLFetcher::POST,
      this);
  url_fetchers_[fetcher] = request_data;
  fetcher->SetAutomaticallyRetryOn5xx(false);
  fetcher->SetRequestContext(request_context);
  fetcher->SetUploadData("text/plain", form_xml);
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DO_NOT_SEND_COOKIES);
  fetcher->Start();

  DVLOG(1) << "Sending AutofillDownloadManager "
           << RequestTypeToString(request_data.request_type)
           << " request: " << form_xml;

  return true;
}

void AutofillDownloadManager::CacheQueryRequest(
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

bool AutofillDownloadManager::CheckCacheForQueryRequest(
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

std::string AutofillDownloadManager::GetCombinedSignature(
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

void AutofillDownloadManager::OnURLFetchComplete(
    const net::URLFetcher* source) {
  std::map<net::URLFetcher *, FormRequestData>::iterator it =
      url_fetchers_.find(const_cast<net::URLFetcher*>(source));
  if (it == url_fetchers_.end()) {
    // Looks like crash on Mac is possibly caused with callback entering here
    // with unknown fetcher when network is refreshed.
    return;
  }
  std::string request_type(RequestTypeToString(it->second.request_type));
  const int kHttpResponseOk = 200;
  const int kHttpInternalServerError = 500;
  const int kHttpBadGateway = 502;
  const int kHttpServiceUnavailable = 503;

  CHECK(it->second.form_signatures.size());
  if (source->GetResponseCode() != kHttpResponseOk) {
    bool back_off = false;
    std::string server_header;
    switch (source->GetResponseCode()) {
      case kHttpBadGateway:
        if (!source->GetResponseHeaders()->EnumerateHeader(NULL, "server",
                                                           &server_header) ||
            StartsWithASCII(server_header.c_str(),
                            kAutofillQueryServerNameStartInHeader,
                            false) != 0)
          break;
        // Bad gateway was received from Autofill servers. Fall through to back
        // off.
      case kHttpInternalServerError:
      case kHttpServiceUnavailable:
        back_off = true;
        break;
    }

    if (back_off) {
      base::Time back_off_time(base::Time::Now() + source->GetBackoffDelay());
      if (it->second.request_type == AutofillDownloadManager::REQUEST_QUERY) {
        next_query_request_ = back_off_time;
      } else {
        next_upload_request_ = back_off_time;
      }
    }

    DVLOG(1) << "AutofillDownloadManager: " << request_type
             << " request has failed with response "
             << source->GetResponseCode();
    observer_->OnServerRequestError(it->second.form_signatures[0],
                                    it->second.request_type,
                                    source->GetResponseCode());
  } else {
    std::string response_body;
    source->GetResponseAsString(&response_body);
    DVLOG(1) << "AutofillDownloadManager: " << request_type
             << " request has succeeded with response body: " << response_body;
    if (it->second.request_type == AutofillDownloadManager::REQUEST_QUERY) {
      CacheQueryRequest(it->second.form_signatures, response_body);
      observer_->OnLoadedServerPredictions(response_body);
    } else {
      double new_positive_upload_rate = 0;
      double new_negative_upload_rate = 0;
      AutofillUploadXmlParser parse_handler(&new_positive_upload_rate,
                                            &new_negative_upload_rate);
      buzz::XmlParser parser(&parse_handler);
      parser.Parse(response_body.data(), response_body.length(), true);
      if (parse_handler.succeeded()) {
        SetPositiveUploadRate(new_positive_upload_rate);
        SetNegativeUploadRate(new_negative_upload_rate);
      }

      observer_->OnUploadedPossibleFieldTypes();
    }
  }
  delete it->first;
  url_fetchers_.erase(it);
}

}  // namespace autofill
