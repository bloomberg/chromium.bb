// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/crw_web_ui_scheme_handler.h"

#include <map>

#import "ios/web/webui/url_fetcher_block_adapter.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWWebUISchemeHandler {
  scoped_refptr<network::SharedURLLoaderFactory> _URLLoaderFactory;
  // Set of live WebUI fetchers for retrieving data.
  std::map<id<WKURLSchemeTask>, std::unique_ptr<web::URLFetcherBlockAdapter>>
      _map;
}

- (instancetype)initWithURLLoaderFactory:
    (scoped_refptr<network::SharedURLLoaderFactory>)URLLoaderFactory {
  self = [super init];
  if (self) {
    _URLLoaderFactory = URLLoaderFactory;
  }
  return self;
}

- (void)webView:(WKWebView*)webView
    startURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  GURL URL = net::GURLWithNSURL(urlSchemeTask.request.URL);
  __weak CRWWebUISchemeHandler* weakSelf = self;
  std::unique_ptr<web::URLFetcherBlockAdapter> adapter =
      std::make_unique<web::URLFetcherBlockAdapter>(
          URL, _URLLoaderFactory,
          ^(NSData* data, web::URLFetcherBlockAdapter* fetcher) {
            CRWWebUISchemeHandler* strongSelf = weakSelf;
            if (!strongSelf ||
                strongSelf.map->find(urlSchemeTask) == strongSelf.map->end()) {
              return;
            }
            NSURLResponse* response =
                [[NSURLResponse alloc] initWithURL:urlSchemeTask.request.URL
                                          MIMEType:@"text/html"
                             expectedContentLength:0
                                  textEncodingName:nil];
            [urlSchemeTask didReceiveResponse:response];
            [urlSchemeTask didReceiveData:data];
            [urlSchemeTask didFinish];
            [weakSelf removeFetcher:fetcher];
          });
  _map.insert(std::make_pair(urlSchemeTask, std::move(adapter)));
  _map.find(urlSchemeTask)->second->Start();
}

- (void)webView:(WKWebView*)webView
    stopURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  auto result = _map.find(urlSchemeTask);
  if (result != _map.end()) {
    _map.erase(result);
  }
}

#pragma mark - Private

// Returns a pointer to the |_map| ivar for strongSelf.
- (std::map<id<WKURLSchemeTask>, std::unique_ptr<web::URLFetcherBlockAdapter>>*)
    map {
  return &_map;
}

// Removes |fetcher| from map of active fetchers.
- (void)removeFetcher:(web::URLFetcherBlockAdapter*)fetcher {
  _map.erase(std::find_if(
      _map.begin(), _map.end(),
      [fetcher](const std::pair<const id<WKURLSchemeTask>,
                                std::unique_ptr<web::URLFetcherBlockAdapter>>&
                    entry) { return entry.second.get() == fetcher; }));
}

@end
