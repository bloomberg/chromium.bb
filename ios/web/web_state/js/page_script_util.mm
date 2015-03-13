// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/page_script_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/public/web_client.h"

namespace web {

namespace {
// Returns an autoreleased string containing the JavaScript to be injected into
// the web view as early as possible. Does not include embedder's script.
NSString* GetWebEarlyPageScript(WebViewType web_view_type) {
  switch (web_view_type) {
    case UI_WEB_VIEW_TYPE:
      return GetPageScript(@"web_bundle_ui");
    case WK_WEB_VIEW_TYPE:
      return GetPageScript(@"web_bundle_wk");
  }
  NOTREACHED();
  return nil;
}
}  // namespace

NSString* GetPageScript(NSString* script_file_name) {
  DCHECK(script_file_name);
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:script_file_name
                                             ofType:@"js"];
  DCHECK(path) << "Script file not found: "
               << base::SysNSStringToUTF8(script_file_name) << ".js";
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error) << "Error fetching script: " << [error.description UTF8String];
  DCHECK(content);
  return content;
}

NSString* GetEarlyPageScript(WebViewType web_view_type) {
  DCHECK(GetWebClient());
  NSString* embedder_page_script =
      GetWebClient()->GetEarlyPageScript(web_view_type);
  DCHECK(embedder_page_script);
  return [GetWebEarlyPageScript(web_view_type)
      stringByAppendingString:embedder_page_script];
}

}  // namespace web
