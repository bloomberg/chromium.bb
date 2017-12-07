// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/page_script_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Make sure that script is injected only once. For example, content of
// WKUserScript can be injected into the same page multiple times
// without notifying WKNavigationDelegate (e.g. after window.document.write
// JavaScript call). Injecting the script multiple times invalidates the
// __gCrWeb.windowId variable and will break the ability to send messages from
// JS to the native code. Wrapping injected script into "if (!injected)" check
// prevents multiple injections into the same page.
NSString* MakeScriptInjectableOnce(NSString* script) {
  NSString* kScriptTemplate = @"if (typeof __gCrWeb !== 'object') { %@; }";
  return [NSString stringWithFormat:kScriptTemplate, script];
}

}  // namespace

namespace web {

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
  DCHECK(!error) << "Error fetching script: "
                 << base::SysNSStringToUTF8(error.description);
  DCHECK(content);
  return content;
}

NSString* GetEarlyPageScriptForMainFrame(BrowserState* browser_state) {
  DCHECK(GetWebClient());
  NSString* embedder_page_script =
      GetWebClient()->GetEarlyPageScriptForMainFrame(browser_state);
  DCHECK(embedder_page_script);

  NSString* web_bundle = GetPageScript(@"main_frame_web_bundle");

  // The WKBackForwardList based navigation manager doesn't need to inject
  // JavaScript to intercept navigation calls.
  if (!GetWebClient()->IsSlimNavigationManagerEnabled()) {
    web_bundle = [NSString
        stringWithFormat:@"%@; %@", web_bundle, GetPageScript(@"nav_bundle")];
  }

  NSString* script =
      [NSString stringWithFormat:@"%@; %@", web_bundle, embedder_page_script];
  return MakeScriptInjectableOnce(script);
}

NSString* GetEarlyPageScriptForAllFrames(BrowserState* browser_state) {
  return MakeScriptInjectableOnce(GetPageScript(@"all_frames_web_bundle"));
}

}  // namespace web
