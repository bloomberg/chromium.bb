// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_DOWNLOAD_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_DOWNLOAD_H_

#include <map>
#include <string>

#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/net/url_fetcher.h"

// Handles getting and updating AutoFill heuristics.
class AutoFillDownloadManager : public URLFetcher::Delegate {
 public:
  // An interface used to notify clients of AutoFillDownloadManager.
  class Observer {
   public:
    // Called when heuristic successfully received from server.
    // |form_signature| - the signature of the requesting form.
    // |heuristic_xml| - server response.
    virtual void OnLoadedAutoFillHeuristics(
        const std::string& form_signature,
        const std::string& heuristic_xml) = 0;
    // Called when heuristic either successfully considered for upload and
    // not send or uploaded.
    // |form_signature| - the signature of the requesting form.
    virtual void OnUploadedAutoFillHeuristics(
        const std::string& form_signature) = 0;
    // Called when there was an error during the request.
    // |form_signature| - the signature of the requesting form.
    // |http_error| - http error code.
    virtual void OnHeuristicsRequestError(const std::string& form_signature,
                                          int http_error) = 0;
   protected:
    virtual ~Observer() {}
  };

  AutoFillDownloadManager();
  virtual ~AutoFillDownloadManager();

  // |observer| - observer to notify on successful completion or error.
  void SetObserver(AutoFillDownloadManager::Observer *observer);

  // Initiates request to AutoFill servers to download/upload heuristics
  // |form_xml| - form structure XML to upload/download.
  // |form_signature| - form signature hash.
  // |query_data| - if true the data is queried and observer notified with new
  // data, if available. If false heuristic data is uploaded to our servers.
  // |form_was_matched| - if |query_data| is false indicates if the form was
  // matched. Ignored otherwise.
  bool StartRequest(const std::string& form_xml,
                    const std::string& form_signature,
                    bool query_data,
                    bool form_was_matched);

  bool CancelRequest(const std::string& form_signature, bool query_data);

 protected:
  // URLFetcher::Delegate implementation:
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 private:
  struct FormRequestData {
    std::string form_signature;
    bool query;
  };

  // For each requested form for both query and upload we create a separate
  // request and save its info. As url fetcher is identified by its address
  // we use a map between fetchers and info.
  std::map<URLFetcher *, FormRequestData> url_fetchers_;
  AutoFillDownloadManager::Observer *observer_;
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_DOWNLOAD_H_

