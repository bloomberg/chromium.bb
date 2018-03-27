// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_JOB_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_JOB_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/offline_pages/core/archive_validator.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "net/url_request/url_request_job.h"

namespace base {
class FilePath;
}

namespace net {
class FileStream;
class IOBuffer;
}  // namespace net

namespace previews {
class PreviewsDecider;
}

namespace offline_pages {

// A request job that serves content from a trusted offline file, located either
// in internal directory or in public directory with digest validated, when a
// http/https URL is being navigated on disconnected or poor network. If no
// trusted offline file can be found, fall back to the default network handling
// which will try to load the live version.
//
// The only header handled by this request job is:
// * "X-Chrome-offline" custom header.
class OfflinePageRequestJob : public net::URLRequestJob {
 public:
  // This enum is used for UMA reporting. It contains all possible outcomes of
  // handling requests that might service offline page in different network
  // conditions. Generally one of these outcomes will happen.
  // The fringe errors (like no OfflinePageModel, etc.) are not reported due
  // to their low probability.
  // NOTE: because this is used for UMA reporting, these values should not be
  // changed or reused; new values should be ended immediately before the MAX
  // value. Make sure to update the histogram enum
  // (OfflinePagesAggregatedRequestResult in enums.xml) accordingly.
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
    DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK,
    DIGEST_MISMATCH_ON_FLAKY_NETWORK,
    DIGEST_MISMATCH_ON_PROHIBITIVELY_SLOW_NETWORK,
    DIGEST_MISMATCH_ON_CONNECTED_NETWORK,
    FILE_NOT_FOUND,
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
    // Launched due to clicking a notification.
    NOTIFICATION = 7,
    // Launched due to processing a file URL intent to view MHTML file.
    FILE_URL_INTENT = 8,
    // Launched due to processing a content URL intent to view MHTML content.
    CONTENT_URL_INTENT = 9,
    COUNT
  };

  enum class NetworkState {
    // No network connection.
    DISCONNECTED_NETWORK,
    // Prohibitively slow means that the NetworkQualityEstimator reported a
    // connection slow enough to warrant showing an offline page if available.
    // This requires offline previews to be enabled and the URL of the request
    // to be allowed by previews.
    PROHIBITIVELY_SLOW_NETWORK,
    // Network error received due to bad network, i.e. connected to a hotspot or
    // proxy that does not have a working network.
    FLAKY_NETWORK,
    // Network is in working condition.
    CONNECTED_NETWORK,
    // Force to load the offline page if it is available, though network is in
    // working condition.
    FORCE_OFFLINE_ON_CONNECTED_NETWORK
  };

  // Describes the info about an offline page candidate.
  struct Candidate {
    OfflinePageItem offline_page;
    // Whether the archive file is in internal directory, for which it can be
    // deemed trusted without validation.
    bool archive_is_in_internal_dir;
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

  class ThreadSafeArchiveValidator final
      : public ArchiveValidator,
        public base::RefCountedThreadSafe<ThreadSafeArchiveValidator> {
   public:
    ThreadSafeArchiveValidator() = default;

   private:
    friend class base::RefCountedThreadSafe<ThreadSafeArchiveValidator>;
    ~ThreadSafeArchiveValidator() override = default;
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
  int ReadRawData(net::IOBuffer* dest, int dest_size) override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  void GetLoadTimingInfo(net::LoadTimingInfo* load_timing_info) const override;
  bool CopyFragmentOnRedirect(const GURL& location) const override;
  bool GetMimeType(std::string* mime_type) const override;
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;

  // Called when offline pages matching the request URL are found. The list is
  // sorted based on creation date in descending order.
  void OnOfflinePagesAvailable(const std::vector<Candidate>& candidates);

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate);

 private:
  enum class FileValidationResult {
    // The file passes the digest validation and thus can be trusted.
    FILE_VALIDATION_SUCCEEDED,
    // The file does not exist.
    FILE_NOT_FOUND,
    // The digest validation fails.
    FILE_VALIDATION_FAILED,
  };

  OfflinePageRequestJob(net::URLRequest* request,
                        net::NetworkDelegate* network_delegate,
                        previews::PreviewsDecider* previews_decider);

  void StartAsync();

  // Restarts the request job in order to fall back to the default handling.
  void FallbackToDefault();

  AccessEntryPoint GetAccessEntryPoint() const;

  const OfflinePageItem& GetCurrentOfflinePage() const;

  bool IsProcessingFileUrlIntent() const;
  bool IsProcessingContentUrlIntent() const;
  bool IsProcessingFileOrContentUrlIntent() const;

  void OnTrustedOfflinePageFound();
  void VisitTrustedOfflinePage();
  void SetOfflinePageNavigationUIData(bool is_offline_page);
  void Redirect(const GURL& redirected_url);

  void OpenFile(const base::FilePath& file_path,
                const net::CompletionCallback& callback);
  void UpdateDigestOnBackground(
      scoped_refptr<net::IOBuffer> buffer,
      size_t len,
      base::OnceCallback<void(void)> digest_updated_callback);
  void FinalizeDigestOnBackground(
      base::OnceCallback<void(const std::string&)> digest_finalized_callback);

  // All the work related to validations.
  void ValidateFile();
  void GetFileSizeForValidation();
  void DidGetFileSizeForValidation(const int64_t* actual_file_size);
  void DidOpenForValidation(int result);
  void ReadForValidation();
  void DidReadForValidation(int result);
  void DidComputeActualDigestForValidation(const std::string& actual_digest);
  void OnFileValidationDone(FileValidationResult result);

  // All the work related to serving from the archive file.
  void DidOpenForServing(int result);
  void DidSeekForServing(int64_t result);
  void DidReadForServing(scoped_refptr<net::IOBuffer> buf, int result);
  void NotifyReadRawDataComplete(int result);
  void DidComputeActualDigestForServing(int result,
                                        const std::string& actual_digest);

  std::unique_ptr<Delegate> delegate_;

  OfflinePageHeader offline_header_;
  NetworkState network_state_;

  // For redirect simulation.
  scoped_refptr<net::HttpResponseHeaders> fake_headers_for_redirect_;
  base::TimeTicks receive_redirect_headers_end_;
  base::Time redirect_response_time_;

  // Used to determine if an URLRequest is eligible for offline previews.
  previews::PreviewsDecider* previews_decider_;

  // To run any file related operations.
  scoped_refptr<base::TaskRunner> file_task_runner_;

  // For file validaton purpose.
  std::vector<Candidate> candidates_;
  size_t candidate_index_;
  scoped_refptr<net::IOBuffer> buffer_;
  scoped_refptr<ThreadSafeArchiveValidator> archive_validator_;

  // For the purpose of serving from the archive file.
  base::FilePath file_path_;
  std::unique_ptr<net::FileStream> stream_;
  bool has_range_header_;

  base::WeakPtrFactory<OfflinePageRequestJob> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestJob);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_JOB_H_
