// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_download.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/profile.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/autofill/internal/autofill_download_internal.h"
#else
#define AUTO_FILL_QUERY_SERVER_REQUEST_URL "http://disabled"
#define AUTO_FILL_UPLOAD_SERVER_REQUEST_URL "http://disabled"
#endif

namespace {
// We only send a fraction of the forms to upload server.
// The rate for positive/negative matches potentially could be different.
const double kAutoFillPositiveUploadRate = 0.01;
const double kAutoFillNegativeUploadRate = 0.01;
}  // namespace

AutoFillDownloadManager::AutoFillDownloadManager()
  : observer_(NULL) {
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

bool AutoFillDownloadManager::StartRequest(const std::string& form_xml,
                                           const std::string& form_signature,
                                           bool query_data,
                                           bool form_was_matched) {
  if (!query_data) {
    // Check if we need to upload form.
    double upload_rate = form_was_matched ? kAutoFillPositiveUploadRate :
      kAutoFillNegativeUploadRate;
    if (base::RandDouble() > upload_rate) {
      LOG(INFO) << "AutoFillDownloadManager: Upload request is ignored";
      if (observer_)
        observer_->OnUploadedAutoFillHeuristics(form_signature);
      return true;
    }
  }

  std::string request_url;
  if (query_data)
    request_url = AUTO_FILL_QUERY_SERVER_REQUEST_URL;
  else
    request_url = AUTO_FILL_UPLOAD_SERVER_REQUEST_URL;

  URLFetcher *fetcher =
      URLFetcher::Create(0, GURL(request_url), URLFetcher::POST, this);
  FormRequestData data;
  data.form_signature = form_signature;
  data.query = query_data;
  url_fetchers_[fetcher] = data;
  fetcher->set_request_context(Profile::GetDefaultRequestContext());
  fetcher->set_upload_data("text/plain", form_xml);
  fetcher->Start();
  return true;
}

bool AutoFillDownloadManager::CancelRequest(const std::string& form_signature,
                                            bool query_data) {
  for (std::map<URLFetcher *, FormRequestData>::iterator it =
       url_fetchers_.begin();
       it != url_fetchers_.end();
       ++it) {
    if (it->second.form_signature == form_signature &&
        it->second.query == query_data) {
      delete it->first;
      url_fetchers_.erase(it);
      return true;
    }
  }
  return false;
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
  std::string type_of_request(it->second.query ? "query" : "upload");
  const int kHttpResponseOk = 200;
  if (response_code != kHttpResponseOk) {
    LOG(INFO) << "AutoFillDownloadManager: " << type_of_request <<
        " request has failed with response" << response_code;
    if (observer_) {
      observer_->OnHeuristicsRequestError(it->second.form_signature,
                                          response_code);
    }
  } else {
    LOG(INFO) << "AutoFillDownloadManager: " << type_of_request <<
        " request has succeeded";
    if (it->second.query) {
      if (observer_)
        observer_->OnLoadedAutoFillHeuristics(it->second.form_signature, data);
    } else {
      if (observer_)
        observer_->OnUploadedAutoFillHeuristics(it->second.form_signature);
    }
  }
}


