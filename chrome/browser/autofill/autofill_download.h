// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_DOWNLOAD_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_DOWNLOAD_H_
#pragma once

#include <stddef.h>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "content/common/net/url_fetcher.h"

class AutofillMetrics;
class FormStructure;
class GURL;
class Profile;

namespace net {
class URLRequestStatus;
}

// Handles getting and updating Autofill heuristics.
class AutofillDownloadManager : public URLFetcher::Delegate {
 public:
  enum AutofillRequestType {
    REQUEST_QUERY,
    REQUEST_UPLOAD,
  };

  // An interface used to notify clients of AutofillDownloadManager.
  // Notifications are *not* guaranteed to be called.
  class Observer {
   public:
    // Called when field type predictions are successfully received from the
    // server.
    // |response_xml| - server response.
    virtual void OnLoadedServerPredictions(const std::string& response_xml) = 0;
    // Called when heuristic either successfully considered for upload and
    // not send or uploaded.
    virtual void OnUploadedPossibleFieldTypes() = 0;
    // Called when there was an error during the request.
    // |form_signature| - the signature of the requesting form.
    // |request_type| - type of request that failed.
    // |http_error| - HTTP error code.
    virtual void OnServerRequestError(const std::string& form_signature,
                                      AutofillRequestType request_type,
                                      int http_error) = 0;
   protected:
    virtual ~Observer() {}
  };

  // |profile| can be NULL in unit-tests only.
  explicit AutofillDownloadManager(Profile* profile);
  virtual ~AutofillDownloadManager();

  // |observer| - observer to notify on successful completion or error.
  void SetObserver(AutofillDownloadManager::Observer* observer);

  // Starts a query request to Autofill servers. The observer is called with the
  // list of the fields of all requested forms.
  // |forms| - array of forms aggregated in this request.
  bool StartQueryRequest(const ScopedVector<FormStructure>& forms,
                         const AutofillMetrics& metric_logger);

  // Starts an upload request for the given |form|, unless throttled by the
  // server. The probability of the request going over the wire is
  // GetPositiveUploadRate() if |form_was_autofilled| is true, or
  // GetNegativeUploadRate() otherwise. The observer will be called even if
  // there was no actual trip over the wire.
  // |available_field_types| should contain the types for which we have data
  // stored on the local client.
  bool StartUploadRequest(const FormStructure& form,
                          bool form_was_autofilled,
                          const FieldTypeSet& available_field_types);

  // Cancels pending request.
  // |form_signature| - signature of the form being cancelled. Warning:
  // for query request if more than one form sent in the request, all other
  // forms will be cancelled as well.
  // |request_type| - type of the request.
  bool CancelRequest(const std::string& form_signature,
                     AutofillRequestType request_type);

 private:
  friend class AutofillDownloadTestHelper;  // unit-test.
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadTest, QueryAndUploadTest);

  struct FormRequestData;
  typedef std::list<std::pair<std::string, std::string> > QueryRequestCache;

  // Initiates request to Autofill servers to download/upload heuristics.
  // |form_xml| - form structure XML to upload/download.
  // |request_data| - form signature hash(es) and indicator if it was a query.
  // |request_data.query| - if true the data is queried and observer notified
  //   with new data, if available. If false heuristic data is uploaded to our
  //   servers.
  bool StartRequest(const std::string& form_xml,
                    const FormRequestData& request_data);

  // Each request is page visited. We store last |max_form_cache_size|
  // request, to avoid going over the wire. Set to 16 in constructor. Warning:
  // the search is linear (newest first), so do not make the constant very big.
  void set_max_form_cache_size(size_t max_form_cache_size) {
    max_form_cache_size_ = max_form_cache_size;
  }

  // Caches query request. |forms_in_query| is a vector of form signatures in
  // the query. |query_data| is the successful data returned over the wire.
  void CacheQueryRequest(const std::vector<std::string>& forms_in_query,
                         const std::string& query_data);
  // Returns true if query is in the cache, while filling |query_data|, false
  // otherwise. |forms_in_query| is a vector of form signatures in the query.
  bool CheckCacheForQueryRequest(const std::vector<std::string>& forms_in_query,
                                 std::string* query_data) const;
  // Concatenates |forms_in_query| into one signature.
  std::string GetCombinedSignature(
      const std::vector<std::string>& forms_in_query) const;

  // URLFetcher::Delegate implementation:
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const net::ResponseCookies& cookies,
                                  const std::string& data);

  // Probability of the form upload. Between 0 (no upload) and 1 (upload all).
  // GetPositiveUploadRate() is for matched forms,
  // GetNegativeUploadRate() for non-matched.
  double GetPositiveUploadRate() const;
  double GetNegativeUploadRate() const;
  void SetPositiveUploadRate(double rate);
  void SetNegativeUploadRate(double rate);

  // Profile for preference storage.
  Profile* profile_;

  // For each requested form for both query and upload we create a separate
  // request and save its info. As url fetcher is identified by its address
  // we use a map between fetchers and info.
  std::map<URLFetcher*, FormRequestData> url_fetchers_;
  AutofillDownloadManager::Observer *observer_;

  // Cached QUERY requests.
  QueryRequestCache cached_forms_;
  size_t max_form_cache_size_;

  // Time when next query/upload requests are allowed. If 50x HTTP received,
  // exponential back off is initiated, so this times will be in the future
  // for awhile.
  base::Time next_query_request_;
  base::Time next_upload_request_;

  // |positive_upload_rate_| is for matched forms,
  // |negative_upload_rate_| for non matched.
  double positive_upload_rate_;
  double negative_upload_rate_;

  // Needed for unit-test.
  int fetcher_id_for_unittest_;
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_DOWNLOAD_H_
