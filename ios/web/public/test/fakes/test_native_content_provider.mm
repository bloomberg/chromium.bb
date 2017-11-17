// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_native_content_provider.h"

#include <map>

#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TestNativeContentProvider {
  std::map<GURL, id<CRWNativeContent>> _nativeContent;
}

- (void)setController:(id<CRWNativeContent>)controller forURL:(const GURL&)URL {
  _nativeContent[URL] = controller;
}

- (BOOL)hasControllerForURL:(const GURL&)URL {
  return _nativeContent.find(URL) != _nativeContent.end();
}

- (id<CRWNativeContent>)controllerForURL:(const GURL&)URL
                                webState:(web::WebState*)webState {
  auto nativeContent = _nativeContent.find(URL);
  if (nativeContent == _nativeContent.end()) {
    return nil;
  }
  return nativeContent->second;
}

- (id<CRWNativeContent>)controllerForURL:(const GURL&)URL
                               withError:(NSError*)error
                                  isPost:(BOOL)isPost {
  NOTREACHED();
  return nil;
}

@end
