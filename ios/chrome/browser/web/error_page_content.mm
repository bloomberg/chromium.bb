// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/error_page_content.h"

#include <memory>

#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/web/error_page_generator.h"
#include "ios/web/public/referrer.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@implementation ErrorPageContent

#pragma mark -
#pragma mark CRWNativeContent methods

// Override StaticHtmlNativeContent method to always force error pages to
// try loading the URL again, because the error might have been fixed.
- (void)reload {
  // Because we don't have the original page transition at this point, just
  // use PAGE_TRANSITION_TYPED. We can revisit if this causes problems.
  [super loadURL:[self url]
               referrer:web::Referrer()
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:YES];
}

- (id)initWithLoader:(id<UrlLoader>)loader
        browserState:(web::BrowserState*)browserState
                 url:(const GURL&)url
               error:(NSError*)error
              isPost:(BOOL)isPost
         isIncognito:(BOOL)isIncognito {
  ErrorPageGenerator* generator =
      [[[ErrorPageGenerator alloc] initWithError:error
                                          isPost:isPost
                                     isIncognito:isIncognito] autorelease];

  base::scoped_nsobject<StaticHtmlViewController> HTMLViewController(
      [[StaticHtmlViewController alloc] initWithGenerator:generator
                                             browserState:browserState]);
  return [super initWithLoader:loader
      staticHTMLViewController:HTMLViewController
                           URL:url];
}

@end
