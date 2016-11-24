// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/metrics_network_client_manager.h"

#import "base/ios/weak_nsobject.h"
#import "base/location.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram.h"
#import "ios/chrome/browser/net/metrics_network_client.h"
#include "ios/web/public/web_thread.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PageLoadTimeRecord ()

@property(nonatomic, readonly) GURL url;
@property(nonatomic, readonly) base::TimeTicks creationTime;
@property(nonatomic, assign) BOOL alreadyCounted;

- (instancetype)initWithURL:(const GURL&)url time:(base::TimeTicks)time;

@end

@implementation PageLoadTimeRecord {
  GURL _url;
  base::TimeTicks _creationTime;
  BOOL _alreadyCounted;
}

@synthesize url = _url;
@synthesize creationTime = _creationTime;
@synthesize alreadyCounted = _alreadyCounted;

- (instancetype)initWithURL:(const GURL&)url time:(base::TimeTicks)time {
  if ((self = [super init])) {
    _url = url;
    _creationTime = time;
  }
  return self;
}

@end

@interface MetricsNetworkClientManager ()

// IO-thread-only methods.
- (void)handlePageLoadStarted:(const GURL&)url;
- (void)handlePageLoadCompleted;

@end

@implementation MetricsNetworkClientManager {
  // Set of page load time objects created. Beyond deallocation and
  // creation, should only be accessed on the IO thread.
  base::scoped_nsobject<NSMutableSet> _pageLoadTimes;
  // Current URL being loaded by the tab that owns this object. Only accessible
  // on the IO thread.
  GURL _pageURL;
}

- (instancetype)init {
  if ((self = [super init])) {
    _pageLoadTimes.reset([NSMutableSet set]);
  }
  return self;
}

- (Class)clientClass {
  return [MetricsNetworkClient class];
}

#pragma mark CRWForwardingNetworkClientFactory methods

- (CRNForwardingNetworkClient*)clientHandlingRequest:
        (const net::URLRequest&)request {
  return [[MetricsNetworkClient alloc] initWithManager:self];
}

#pragma mark - public UI-thread methods

- (void)pageLoadStarted:(GURL)url {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE, base::BindBlockArc(^{
                             [self handlePageLoadStarted:url];
                           }));
}

- (void)pageLoadCompleted {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE, base::BindBlockArc(^{
                             [self handlePageLoadCompleted];
                           }));
}

#pragma mark - public IO-thread methods

- (PageLoadTimeRecord*)recordForPageLoad:(const GURL&)url
                                    time:(base::TimeTicks)time {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  base::scoped_nsobject<PageLoadTimeRecord> plt;
  if (!_pageURL.spec().empty() && url == _pageURL) {
    plt.reset([[PageLoadTimeRecord alloc] initWithURL:url time:time]);
    [_pageLoadTimes addObject:plt];
  }
  return plt.get();
}

#pragma mark - IO-thread handlers for UI thread methods.

- (void)handlePageLoadStarted:(const GURL&)url {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  [_pageLoadTimes removeAllObjects];
  _pageURL = url;
}

- (void)handlePageLoadCompleted {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  for (PageLoadTimeRecord* plt in _pageLoadTimes.get()) {
    if (plt.url == _pageURL && !plt.alreadyCounted) {
      plt.alreadyCounted = YES;
      base::TimeDelta elapsed = base::TimeTicks::Now() - plt.creationTime;
      UMA_HISTOGRAM_MEDIUM_TIMES("Tabs.iOS_PostRedirectPLT", elapsed);
      break;
    }
  }
}

@end
