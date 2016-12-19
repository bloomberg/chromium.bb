// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/error_page_content.h"

#include <memory>

#import "base/ios/ns_error_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/error_page/common/error_page_params.h"
#include "components/error_page/common/localized_error.h"
#include "components/grit/components_resources.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/web/public/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
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

- (void)generateHtml:(HtmlCallback)callback {
  callback(html_.get());
}

- (id)initWithLoader:(id<UrlLoader>)loader
        browserState:(web::BrowserState*)browserState
                 url:(const GURL&)url
               error:(NSError*)error
              isPost:(BOOL)isPost
         isIncognito:(BOOL)isIncognito {
  NSString* badURLString =
      [[error userInfo] objectForKey:NSURLErrorFailingURLStringErrorKey];
  NSError* originalError = base::ios::GetFinalUnderlyingErrorFromError(error);
  NSString* errorDomain = nil;
  int errorCode = 0;

  if (originalError) {
    errorDomain = [originalError domain];
    errorCode = [originalError code];
  } else {
    errorDomain = [error domain];
    errorCode = [error code];
  }
  DCHECK(errorCode != 0);

  base::DictionaryValue errorStrings;
  error_page::LocalizedError::GetStrings(
      errorCode, base::SysNSStringToUTF8(errorDomain),
      GURL(base::SysNSStringToUTF16(badURLString)), isPost, false, false,
      isIncognito, GetApplicationContext()->GetApplicationLocale(), nullptr,
      &errorStrings);

  ui::ScaleFactor scaleFactor =
      ResourceBundle::GetSharedInstance().GetMaxScaleFactor();

  const base::StringPiece templateHTML(
      ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
          IDR_NET_ERROR_HTML, scaleFactor));
  if (templateHTML.empty())
    NOTREACHED() << "unable to load template. ID: " << IDR_NET_ERROR_HTML;
  std::string errorHTML = webui::GetTemplatesHtml(
      templateHTML, &errorStrings, "t" /* IDR_NET_ERROR_HTML root id */);
  html_.reset([base::SysUTF8ToNSString(errorHTML) retain]);

  base::scoped_nsobject<StaticHtmlViewController> HTMLViewController(
      [[StaticHtmlViewController alloc] initWithGenerator:self
                                             browserState:browserState]);
  return [super initWithLoader:loader
      staticHTMLViewController:HTMLViewController
                           URL:url];
}

@end
