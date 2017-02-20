// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/error_page_generator.h"

#import "base/ios/ns_error_util.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/error_page/common/error_page_params.h"
#include "components/error_page/common/localized_error.h"
#include "components/grit/components_resources.h"
#include "ios/chrome/browser/application_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/scale_factor.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "url/gurl.h"

@implementation ErrorPageGenerator {
  // Stores the HTML generated from the NSError in the initializer.
  base::scoped_nsobject<NSString> html_;
}

- (instancetype)initWithError:(NSError*)error
                       isPost:(BOOL)isPost
                  isIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    NSString* badURLSpec = error.userInfo[NSURLErrorFailingURLStringErrorKey];
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
        GURL(base::SysNSStringToUTF16(badURLSpec)), isPost, false, false,
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
  }
  return self;
}

#pragma mark - HtmlGenerator

- (void)generateHtml:(HtmlCallback)callback {
  callback(html_.get());
}

@end
