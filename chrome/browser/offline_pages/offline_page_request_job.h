// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_JOB_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_JOB_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "net/url_request/url_request_file_job.h"

namespace base {
class FilePath;
}

namespace previews {
class PreviewsDecider;
}

namespace offline_pages {

// A request job that serves content from offline file.
class OfflinePageRequestJob : public net::URLRequestFileJob {
 public:
  // This enum is used for UMA reporting. It contains all possible outcomes of
  // handling requests that might service offline page in different network
  // conditions. Generally one of these outcomes will happen.
  // The fringe errors (like no OfflinePageModel, etc.) are not reported due
  // to their low probability.
  // NOTE: because this is used for UMA reporting, these values should not be
  // changed or reused; new values should be ended immediately before the MAX
  // value. Make sure to update the histogram enum
  // (OfflinePagesAggregatedRequestResult in histograms.xml) accordingly.
  // Public for testing.
  enum class AggregatedRequestResult {
    SHOW_OFFLINE_ON_DISCONNECTED_NETWORK,
    PAGE_NOT_FOUND_ON_DISCONNECTED_NETWORK,
    SHOW_OFFLINE_ON_FLAKY_NETWORK,
    PAGE_NOT_FOUND_ON_FLAKY_NETWORK,
    SHOW_OFFLINE_ON_PROHIBITIVELY_SLOW_NETWORK,
    PAGE_NOT_FOUND_ON_PROHIBITIVELY_SLOW_NETWORK,
    PAGE_NOT_FRESH_ON_PROHIBITIVELY_SLOW_NETWORK,
    SHOW_OFFLINE_ON_CONNECTED_NETWORK,
    PAGE_NOT_FOUND_ON_CONNECTED_NETWORK,
    NO_TAB_ID,
    NO_WEB_CONTENTS,
    SHOW_NET_ERROR_PAGE,
    REDIRECTED_ON_DISCONNECTED_NETWORK,
    REDIRECTED_ON_FLAKY_NETWORK,
    REDIRECTED_ON_PROHIBITIVELY_SLOW_NETWORK,
    REDIRECTED_ON_CONNECTED_NETWORK,
    AGGREGATED_REQUEST_RESULT_MAX
  };

  // This enum is used for UMA reporting of the UI location from which an
  // offline page was launched.
  // NOTE: because this is used for UMA reporting, these values should not be
  // changed or reused; new values should be appended immediately before the MAX
  // value. Make sure to update the histogram enum (OfflinePagesAcessEntryPoint
  // in enums.xml) accordingly.
  enum class AccessEntryPoint {
    // Any other cases not listed below.
    UNKNOWN = 0,
    // Launched from the NTP suggestions or bookmarks.
    NTP_SUGGESTIONS_OR_BOOKMARKS = 1,
    // Launched from Downloads home.
    DOWNLOADS = 2,
    // Launched from the omnibox.
    OMNIBOX = 3,
    // Launched from Chrome Custom Tabs.
    CCT = 4,
    // Launched due to clicking a link in a page.
    LINK = 5,
    // Launched due to hitting the reload button or hitting enter in the
    // omnibox.
    RELOAD = 6,
    COUNT
  };

  // Delegate that allows tests to overwrite certain behaviors.
  class Delegate {
   public:
    using TabIdGetter = base::Callback<bool(content::WebContents*, int*)>;

    virtual ~Delegate() {}

    virtual content::ResourceRequestInfo::WebContentsGetter
    GetWebContentsGetter(net::URLRequest* request) const = 0;

    virtual TabIdGetter GetTabIdGetter() const = 0;
  };

  // Reports the aggregated result combining both request result and network
  // state.
  static void ReportAggregatedRequestResult(AggregatedRequestResult result);

  // Creates and returns a job to serve the offline page. Nullptr is returned if
  // offline page cannot or should not be served. Embedder must gaurantee that
  // |previews_decider| outlives the returned instance.
  static OfflinePageRequestJob* Create(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      previews::PreviewsDecider* previews_decider);

  ~OfflinePageRequestJob() override;

  // net::URLRequestJob overrides:
  void Start() override;
  void Kill() override;
  bool IsRedirectResponse(GURL* location, int* http_status_code) override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  void GetLoadTimingInfo(net::LoadTimingInfo* load_timing_info) const override;
  bool CopyFragmentOnRedirect(const GURL& location) const override;

  // net::URLRequestFileJob overrides:
  void OnOpenComplete(int result) override;
  void OnSeekComplete(int64_t result) override;
  void OnReadComplete(net::IOBuffer* buf, int result) override;

  void OnOfflineFilePathAvailable(const std::string& name_space,
                                  const base::FilePath& offline_file_path);
  void OnOfflineRedirectAvailabe(const GURL& redirected_url);

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate);

 private:
  // net::URLRequestFileJob overrides:
  bool CanAccessFile(const base::FilePath& original_path,
                     const base::FilePath& absolute_path) override;

  OfflinePageRequestJob(net::URLRequest* request,
                        net::NetworkDelegate* network_delegate,
                        previews::PreviewsDecider* previews_decider);

  void StartAsync();

  // Restarts the request job in order to fall back to the default handling.
  void FallbackToDefault();

  AccessEntryPoint GetAccessEntryPoint() const;

  std::unique_ptr<Delegate> delegate_;

  // For redirect simulation.
  scoped_refptr<net::HttpResponseHeaders> fake_headers_for_redirect_;
  base::TimeTicks receive_redirect_headers_end_;
  base::Time redirect_response_time_;

  // Used to determine if an URLRequest is eligible for offline previews.
  previews::PreviewsDecider* previews_decider_;

  base::WeakPtrFactory<OfflinePageRequestJob> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestJob);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_JOB_H_
