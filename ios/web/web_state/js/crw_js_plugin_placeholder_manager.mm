// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/crw_js_plugin_placeholder_manager.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWJSPluginPlaceholderManager

namespace {

// Returns a string with \ and ' escaped, and wrapped in '.
// This is used instead of GetQuotedJSONString because that will convert
// UTF-16 to UTF-8, which can cause problems when injecting scripts depending
// on the page encoding (see crbug.com/302741).
NSString* EscapedQuotedString(NSString* string) {
  string = [string stringByReplacingOccurrencesOfString:@"\\"
                                             withString:@"\\\\"];
  string = [string stringByReplacingOccurrencesOfString:@"'"
                                             withString:@"\\'"];
  return [NSString stringWithFormat:@"'%@'", string];
}

}

#pragma mark -
#pragma mark ProtectedMethods

- (NSString*)scriptPath {
  return @"plugin_placeholder";
}

- (NSString*)staticInjectionContent {
  NSString* baseContent = [super staticInjectionContent];
  DCHECK(web::GetWebClient());
  NSString* pluginNotSupportedText = base::SysUTF16ToNSString(
      web::GetWebClient()->GetPluginNotSupportedText());
  NSString* placeholderCall = [NSString stringWithFormat:
      @"__gCrWeb.plugin.addPluginPlaceholders(%@);",
          EscapedQuotedString(pluginNotSupportedText)];
  return [baseContent stringByAppendingString:placeholderCall];
}

@end
