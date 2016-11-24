// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/metrics_network_client.h"

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/net/metrics_network_client_manager.h"
#include "ios/web/public/url_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

@interface MetricsNetworkClient () {
  BOOL _histogramUpdated;
  // Pointer to the load time record for this request. This will be created
  // and owned by |_manager|, and it will remain valid as long as |_manager| is.
  __unsafe_unretained PageLoadTimeRecord* _loadTimeRecord;
  // Pointer to the creating manager, which is owned by a tab. All network
  // requests for the tab are destroyed before the tab is, so this pointer
  // will always be valid as long as the owning client is alive.
  __unsafe_unretained MetricsNetworkClientManager* _manager;
  // A pointer to the request, kept so it can be referred to later.
  scoped_refptr<net::HttpResponseHeaders> _nativeHeaders;
}
- (void)updateHistogram:(NSInteger)code;
@end

@implementation MetricsNetworkClient

- (void)updateHistogram:(NSInteger)code {
  DCHECK(!_histogramUpdated) << "Histogram should not be updated twice.";
  // The |error| must be in the |net::kErrorDomain|. All those errors are
  // defined in |net/base/net_error_list.h|, must be negative values that
  // fits in an |int|. See |net/base/net_errors.h| for more information.
  DCHECK_LE(code, 0) << "Net error codes should be negative.";
  DCHECK_GT(code, INT_MIN) << "Net error code should fit in an int.";
  // On iOS, we cannot distinguish between main frames, images and other
  // subresources. Consequently, all the codes are aggregated in
  // |ErrorCodesForMainFrame3|.
  // The other histograms (such as |ErrorCodesForHTTPSGoogleMainFrame2|,
  // |ErrorCodesForImages| and |ErrorCodesForSubresources2|) are not filled.
  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.ErrorCodesForMainFrame3",
                              static_cast<int>(-code));
  _histogramUpdated = YES;
}

- (instancetype)initWithManager:(MetricsNetworkClientManager*)manager {
  if ((self = [super init])) {
    _manager = manager;
  }
  return self;
}

#pragma mark CRNNetworkClientProtocol methods

- (void)didCreateNativeRequest:(net::URLRequest*)nativeRequest {
  [super didCreateNativeRequest:nativeRequest];
  GURL url = web::GURLByRemovingRefFromGURL(nativeRequest->original_url());
  _loadTimeRecord =
      [_manager recordForPageLoad:url time:nativeRequest->creation_time()];
  _nativeHeaders = nativeRequest->response_headers();
}

- (void)didFailWithNSErrorCode:(NSInteger)nsErrorCode
                  netErrorCode:(int)netErrorCode {
  [super didFailWithNSErrorCode:nsErrorCode netErrorCode:netErrorCode];

  // Ignore NSURLErrorCancelled errors, which are sometimes created to
  // silently abort loads.

  if (nsErrorCode != NSURLErrorCancelled) {
    [self updateHistogram:netErrorCode];
  }
}

- (void)didFinishLoading {
  [super didFinishLoading];
  [self updateHistogram:net::OK];
}

@end
