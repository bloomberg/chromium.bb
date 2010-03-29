// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_download.h"

#include <algorithm>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/profile.h"

#define DISABLED_REQUEST_URL "http://disabled"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/autofill/internal/autofill_download_internal.h"
#else
#define AUTO_FILL_QUERY_SERVER_REQUEST_URL DISABLED_REQUEST_URL
#define AUTO_FILL_UPLOAD_SERVER_REQUEST_URL DISABLED_REQUEST_URL
#endif

namespace {
// We only send a fraction of the forms to upload server.
// The rate for positive/negative matches potentially could be different.
const double kAutoFillPositiveUploadRate = 0.01;
const double kAutoFillNegativeUploadRate = 0.01;
}  // namespace

AutoFillDownloadManager::AutoFillDownloadManager()
    : observer_(NULL),
      positive_upload_rate_(kAutoFillPositiveUploadRate),
      negative_upload_rate_(kAutoFillNegativeUploadRate),
      fetcher_id_for_unittest_(0),
      is_testing_(false) {
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
    const ScopedVector<FormStructure>& forms) {
  std::string form_xml;
  FormStructure::EncodeQueryRequest(forms, &form_xml);

  FormRequestData request_data;
  request_data.form_signatures.reserve(forms.size());
  for (ScopedVector<FormStructure>::const_iterator it = forms.begin();
       it != forms.end();
       ++it) {
    request_data.form_signatures.push_back((*it)->FormSignature());
  }

  request_data.request_type = AutoFillDownloadManager::REQUEST_QUERY;

  return StartRequest(form_xml, request_data);
}

bool AutoFillDownloadManager::StartUploadRequest(
    const FormStructure& form, bool form_was_matched) {
  // Check if we need to upload form.
  // TODO(georgey): adjust this values from returned XML.
  double upload_rate = form_was_matched ? positive_upload_rate_ :
      negative_upload_rate_;
  if (base::RandDouble() > upload_rate) {
    LOG(INFO) << "AutoFillDownloadManager: Upload request is ignored";
    if (observer_)
      observer_->OnUploadedAutoFillHeuristics(form.FormSignature());
    return true;
  }
  std::string form_xml;
  form.EncodeUploadRequest(form_was_matched, &form_xml);

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
  fetcher->set_request_context(Profile::GetDefaultRequestContext());
  fetcher->set_upload_data("text/plain", form_xml);
  fetcher->Start();
  return true;
}

void AutoFillDownloadManager::OnURLFetchComplete(const URLFetcher* source,
                                                 const GURL& url,
                                                 const URLRequestStatus& status,
                                                 int response_code,
                                                 const ResponseCookies& cookies,
                                                 const std::string& data) {
  std::map<URLFetcher *, FormRequestData>::iterator it =
      url_fetchers_.find(const_cast<URLFetcher*>(source));
  DCHECK(it != url_fetchers_.end());
  std::string type_of_request(
      it->second.request_type == AutoFillDownloadManager::REQUEST_QUERY ?
          "query" : "upload");
  const int kHttpResponseOk = 200;
  DCHECK(it->second.form_signatures.size());
  if (response_code != kHttpResponseOk) {
    LOG(INFO) << "AutoFillDownloadManager: " << type_of_request <<
        " request has failed with response" << response_code;
    if (observer_) {
      observer_->OnHeuristicsRequestError(it->second.form_signatures[0],
                                          it->second.request_type,
                                          response_code);
    }
  } else {
    LOG(INFO) << "AutoFillDownloadManager: " << type_of_request <<
        " request has succeeded";
    if (it->second.request_type == AutoFillDownloadManager::REQUEST_QUERY) {
      if (observer_)
        observer_->OnLoadedAutoFillHeuristics(it->second.form_signatures, data);
    } else {
      // TODO(georgey): adjust upload probabilities.
      if (observer_)
        observer_->OnUploadedAutoFillHeuristics(it->second.form_signatures[0]);
    }
  }
  delete it->first;
  url_fetchers_.erase(it);
}


