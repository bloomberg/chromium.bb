// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/url_request/url_fetcher_delegate.h"

class GURL;
class PrefService;

namespace base {
class SequencedTaskRunner;
class TimeDelta;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
class URLRequestStatus;
}

namespace data_reduction_proxy {

typedef base::Callback<void(const std::string&, const net::URLRequestStatus&)>
    FetcherResponseCallback;

class DataReductionProxySettings;
class DataReductionProxyStatisticsPrefs;


// Contains and initializes all Data Reduction Proxy objects that have a
// lifetime based on the UI thread.
class DataReductionProxyService : public base::NonThreadSafe,
                                  public net::URLFetcherDelegate {
 public:
  // The caller must ensure that |settings| and |request_context| remain alive
  // for the lifetime of the |DataReductionProxyService| instance. This instance
  // will take ownership of |statistics_prefs|.
  // TODO(jeremyim): DataReductionProxyService should own
  // DataReductionProxySettings and not vice versa.
  DataReductionProxyService(
      scoped_ptr<DataReductionProxyStatisticsPrefs> statistics_prefs,
      DataReductionProxySettings* settings,
      net::URLRequestContextGetter* request_context_getter);

  ~DataReductionProxyService() override;

  void Shutdown();

  // Requests the given |secure_proxy_check_url|. Upon completion, returns the
  // results to the caller via the |fetcher_callback|. Virtualized for unit
  // testing.
  virtual void SecureProxyCheck(const GURL& secure_proxy_check_url,
                                FetcherResponseCallback fetcher_callback);

  // Constructs statistics prefs. This should not be called if a valid
  // statistics prefs is passed into the constructor.
  void EnableCompressionStatisticsLogging(
      PrefService* prefs,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      const base::TimeDelta& commit_delay);

  DataReductionProxyStatisticsPrefs* statistics_prefs() const {
    return statistics_prefs_.get();
  }

  DataReductionProxySettings* settings() const {
    return settings_;
  }

  base::WeakPtr<DataReductionProxyService> GetWeakPtr();

 protected:
  // Virtualized for testing. Returns a fetcher to check if it is permitted to
  // use the secure proxy.
  virtual net::URLFetcher* GetURLFetcherForSecureProxyCheck(
      const GURL& secure_proxy_check_url);

 private:
  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  net::URLRequestContextGetter* url_request_context_getter_;

  // The URLFetcher being used for the secure proxy check.
  scoped_ptr<net::URLFetcher> fetcher_;
  FetcherResponseCallback fetcher_callback_;

  // Tracks compression statistics to be displayed to the user.
  scoped_ptr<DataReductionProxyStatisticsPrefs> statistics_prefs_;

  DataReductionProxySettings* settings_;

  base::WeakPtrFactory<DataReductionProxyService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyService);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_
