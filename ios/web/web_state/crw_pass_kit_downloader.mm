// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/crw_pass_kit_downloader.h"

#include <memory>

#include "base/mac/scoped_block.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using net::URLFetcher;
using net::URLFetcherDelegate;
using net::URLRequestContextGetter;

namespace {

// Key of the UMA Download.IOSDownloadPassKitResult histogram.
const char kUMADownloadPassKitResult[] = "Download.IOSDownloadPassKitResult";

// Enum for the Download.IOSDownloadPassKitResult UMA histogram to report the
// results of the PassKit download.
// Note: This enum is used to back an UMA histogram, and should be treated as
// append-only.
enum DownloadPassKitResult {
  DOWNLOAD_PASS_KIT_SUCCESSFUL = 0,
  // PassKit download failed for a reason other than wrong MIME type or 401/403
  // HTTP response.
  DOWNLOAD_PASS_KIT_OTHER_FAILURE,
  // PassKit download failed due to either a 401 or 403 HTTP response.
  DOWNLOAD_PASS_KIT_UNAUTHORIZED_FAILURE,
  // PassKit download did not download the correct MIME type. This can happen
  // when web server redirects to login page instead of returning PassKit data.
  DOWNLOAD_PASS_KIT_WRONG_MIME_TYPE_FAILURE,
  DOWNLOAD_PASS_KIT_RESULT_COUNT
};
}  // namespace

@interface CRWPassKitDownloader ()

// The method called by PassKitFetcherDelegate when the download is complete.
// If data is successfully downloaded, it converts the response to
// NSData and passes the result to |_completionHandler|.
- (void)didFinishDownload;

// Reports Download.IOSDownloadPassKitResult UMA metric.
- (void)reportUMAPassKitResult:(DownloadPassKitResult)result;

@end

namespace {

// A delegate for the URLFetcher to tell the CRWPassKitDownloader that the
// download is complete.
class PassKitFetcherDelegate : public URLFetcherDelegate {
 public:
  explicit PassKitFetcherDelegate(CRWPassKitDownloader* owner)
      : owner_(owner) {}
  void OnURLFetchComplete(const URLFetcher* source) override {
    [owner_ didFinishDownload];
  }

 private:
  __weak CRWPassKitDownloader* owner_;
  DISALLOW_COPY_AND_ASSIGN(PassKitFetcherDelegate);
};

}  // namespace

@implementation CRWPassKitDownloader {
  // Completion handler that is called when PassKit data is downloaded.
  base::mac::ScopedBlock<web::PassKitCompletionHandler> _completionHandler;

  // URLFetcher with which PassKit data is downloaded. It is initialized
  // whenever |downloadPassKitFileWithURL| is called.
  std::unique_ptr<URLFetcher> _fetcher;

  // Delegate to bridge between URLFetcher callback and CRWPassKitDownlaoder.
  std::unique_ptr<PassKitFetcherDelegate> _fetcherDelegate;

  // Context getter which is passed to the URLFetcher, as required by
  // URLFetcher API.
  scoped_refptr<URLRequestContextGetter> _requestContextGetter;
}

#pragma mark - Public Methods

- (instancetype)initWithContextGetter:(net::URLRequestContextGetter*)getter
                    completionHandler:(web::PassKitCompletionHandler)handler {
  self = [super init];
  if (self) {
    DCHECK(getter);
    DCHECK(handler);
    _completionHandler.reset([handler copy]);
    _fetcherDelegate.reset(new PassKitFetcherDelegate(self));
    _requestContextGetter = getter;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (BOOL)isMIMETypePassKitType:(NSString*)MIMEType {
  return [MIMEType isEqualToString:@"application/vnd.apple.pkpass"];
}

- (void)downloadPassKitFileWithURL:(const GURL&)URL {
  _fetcher = URLFetcher::Create(URL, URLFetcher::GET, _fetcherDelegate.get());
  _fetcher->SetRequestContext(_requestContextGetter.get());
  _fetcher->Start();
}

- (void)cancelPendingDownload {
  _fetcher.reset();
}

#pragma mark - Private Methods

- (void)didFinishDownload {
  int responseCode = _fetcher->GetResponseCode();
  std::string response;
  // If the download failed, pass nil to |_completionHandler| and log which
  // kind of failure it was.
  if (!_fetcher->GetStatus().is_success() || responseCode != 200 ||
      !_fetcher->GetResponseAsString(&response)) {
    DownloadPassKitResult errorType =
        (responseCode == 401 || responseCode == 403)
            ? DOWNLOAD_PASS_KIT_UNAUTHORIZED_FAILURE
            : DOWNLOAD_PASS_KIT_OTHER_FAILURE;
    [self reportUMAPassKitResult:errorType];
    _completionHandler.get()(nil);
    return;
  }
  std::string MIMEType;
  _fetcher->GetResponseHeaders()->GetMimeType(&MIMEType);
  NSString* convertedMIMEType = base::SysUTF8ToNSString(MIMEType);
  // Verify for logging purposes that the data actually is PassKit data. The
  // completion handler is responsible for displaying the appropriate error
  // message if it isn't. This error case can occur when web server redirects to
  // another page instead of returning PassKit data.
  DownloadPassKitResult successOrFailureLogging =
      ([self isMIMETypePassKitType:convertedMIMEType])
          ? DOWNLOAD_PASS_KIT_SUCCESSFUL
          : DOWNLOAD_PASS_KIT_WRONG_MIME_TYPE_FAILURE;
  [self reportUMAPassKitResult:successOrFailureLogging];
  NSData* data =
      [NSData dataWithBytes:response.c_str() length:response.length()];
  _completionHandler.get()(data);
}

- (void)reportUMAPassKitResult:(DownloadPassKitResult)result {
  UMA_HISTOGRAM_ENUMERATION(kUMADownloadPassKitResult, result,
                            DOWNLOAD_PASS_KIT_RESULT_COUNT);
}

@end
