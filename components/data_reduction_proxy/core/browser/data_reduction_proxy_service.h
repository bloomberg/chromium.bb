// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
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

class DataReductionProxyCompressionStats;
class DataReductionProxyIOData;
class DataReductionProxyServiceObserver;
class DataReductionProxySettings;

// Contains and initializes all Data Reduction Proxy objects that have a
// lifetime based on the UI thread.
class DataReductionProxyService : public base::NonThreadSafe,
                                  public net::URLFetcherDelegate {
 public:
  // The caller must ensure that |settings| and |request_context| remain alive
  // for the lifetime of the |DataReductionProxyService| instance. This instance
  // will take ownership of |compression_stats|.
  // TODO(jeremyim): DataReductionProxyService should own
  // DataReductionProxySettings and not vice versa.
  DataReductionProxyService(
      scoped_ptr<DataReductionProxyCompressionStats> compression_stats,
      DataReductionProxySettings* settings,
      net::URLRequestContextGetter* request_context_getter,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ~DataReductionProxyService() override;

  // Sets the DataReductionProxyIOData weak pointer.
  void SetIOData(base::WeakPtr<DataReductionProxyIOData> io_data);

  void Shutdown();

  // Indicates whether |this| has been fully initialized. |SetIOData| is the
  // final step in initialization.
  bool Initialized() const;

  // Requests the given |secure_proxy_check_url|. Upon completion, returns the
  // results to the caller via the |fetcher_callback|. Virtualized for unit
  // testing.
  virtual void SecureProxyCheck(const GURL& secure_proxy_check_url,
                                FetcherResponseCallback fetcher_callback);

  // Constructs compression stats. This should not be called if a valid
  // compression stats is passed into the constructor.
  void EnableCompressionStatisticsLogging(
      PrefService* prefs,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      const base::TimeDelta& commit_delay);

  // Records daily data savings statistics in |compression_stats_|.
  void UpdateContentLengths(int64 received_content_length,
                            int64 original_content_length,
                            bool data_reduction_proxy_enabled,
                            DataReductionProxyRequestType request_type);

  // Records whether the Data Reduction Proxy is unreachable or not.
  void SetUnreachable(bool unreachable);

  // Bridge methods to safely call to the UI thread objects.
  // Virtual for testing.
  virtual void SetProxyPrefs(bool enabled,
                             bool alternative_enabled,
                             bool at_startup);

  // Methods for adding/removing observers on |this|.
  void AddObserver(DataReductionProxyServiceObserver* observer);
  void RemoveObserver(DataReductionProxyServiceObserver* observer);

  // Accessor methods.
  DataReductionProxyCompressionStats* compression_stats() const {
    return compression_stats_.get();
  }

  DataReductionProxySettings* settings() const {
    return settings_;
  }

  net::URLRequestContextGetter* url_request_context_getter() const {
    return url_request_context_getter_;
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
  scoped_ptr<DataReductionProxyCompressionStats> compression_stats_;

  DataReductionProxySettings* settings_;

  // Used to post tasks to |io_data_|.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // A weak pointer to DataReductionProxyIOData so that UI based objects can
  // make calls to IO based objects.
  base::WeakPtr<DataReductionProxyIOData> io_data_;

  ObserverList<DataReductionProxyServiceObserver> observer_list_;

  bool initialized_;

  base::WeakPtrFactory<DataReductionProxyService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyService);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_
