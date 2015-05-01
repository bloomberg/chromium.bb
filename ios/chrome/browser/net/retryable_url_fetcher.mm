// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/retryable_url_fetcher.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

@interface RetryableURLFetcher ()
- (void)urlFetchDidComplete:(const net::URLFetcher*)fetcher;
@end

class URLRequestDelegate : public net::URLFetcherDelegate {
 public:
  explicit URLRequestDelegate(RetryableURLFetcher* owner) : owner_(owner) {}
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    [owner_ urlFetchDidComplete:source];
  }

 private:
  RetryableURLFetcher* owner_;  // Weak.
};

@implementation RetryableURLFetcher {
  scoped_refptr<net::URLRequestContextGetter> requestContextGetter_;
  scoped_ptr<URLRequestDelegate> fetcherDelegate_;
  scoped_ptr<net::URLFetcher> fetcher_;
  scoped_ptr<net::BackoffEntry> backoffEntry_;
  int retryCount_;
  id<RetryableURLFetcherDelegate> delegate_;  // Weak.
}

- (instancetype)
    initWithRequestContextGetter:(net::URLRequestContextGetter*)context
                        delegate:(id<RetryableURLFetcherDelegate>)delegate
                   backoffPolicy:(const net::BackoffEntry::Policy*)policy {
  self = [super init];
  if (self) {
    DCHECK(context);
    DCHECK(delegate);
    requestContextGetter_ = context;
    delegate_ = delegate;
    if (policy)
      backoffEntry_.reset(new net::BackoffEntry(policy));
  }
  return self;
}

- (void)startFetch {
  DCHECK(requestContextGetter_.get());
  GURL url(base::SysNSStringToUTF8([delegate_ urlToFetch]));
  if (url.is_valid()) {
    fetcherDelegate_.reset(new URLRequestDelegate(self));
    fetcher_ = net::URLFetcher::Create(url, net::URLFetcher::GET,
                                       fetcherDelegate_.get());
    fetcher_->SetRequestContext(requestContextGetter_.get());
    fetcher_->Start();
  }
}

- (int)failureCount {
  return backoffEntry_ ? backoffEntry_->failure_count() : 0;
}

- (void)urlFetchDidComplete:(const net::URLFetcher*)fetcher {
  BOOL success = fetcher->GetResponseCode() == net::HTTP_OK;
  if (!success && backoffEntry_) {
    backoffEntry_->InformOfRequest(false);
    double nextRetry = backoffEntry_->GetTimeUntilRelease().InSecondsF();
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, nextRetry * NSEC_PER_SEC),
                   dispatch_get_main_queue(), ^{
                     [self startFetch];
                   });
    return;
  }
  NSString* response = nil;
  if (success) {
    std::string responseString;
    if (fetcher->GetResponseAsString(&responseString))
      response = base::SysUTF8ToNSString(responseString);
  }
  [delegate_ processSuccessResponse:response];
}

@end
