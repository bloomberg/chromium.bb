// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/mac/mac_util.h"
#include "chrome/browser/platform_util.h"
#import "third_party/mozilla/NSWorkspace+Utils.h"

// Sets Chromium as default browser (only for current user). Returns false if
// this operation fails.
bool ShellIntegration::SetAsDefaultBrowser() {
  if (!platform_util::CanSetAsDefaultBrowser())
    return false;

  // We really do want the main bundle here, not base::mac::MainAppBundle(),
  // which is the bundle for the framework.
  NSString* identifier = [[NSBundle mainBundle] bundleIdentifier];
  [[NSWorkspace sharedWorkspace] setDefaultBrowserWithIdentifier:identifier];
  return true;
}

namespace {

// Returns true if |identifier| is the bundle id of the default browser.
bool IsIdentifierDefaultBrowser(NSString* identifier) {
  NSString* defaultBrowser =
      [[NSWorkspace sharedWorkspace] defaultBrowserIdentifier];
  if (!defaultBrowser)
    return false;
  // We need to ensure we do the comparison case-insensitive as LS doesn't
  // persist the case of our bundle id.
  NSComparisonResult result =
    [defaultBrowser caseInsensitiveCompare:identifier];
  return result == NSOrderedSame;
}

}  // namespace

// Attempt to determine if this instance of Chrome is the default browser and
// return the appropriate state. (Defined as being the handler for HTTP/HTTPS
// protocols; we don't want to report "no" here if the user has simply chosen
// to open HTML files in a text editor and FTP links with an FTP client.)
ShellIntegration::DefaultBrowserState ShellIntegration::IsDefaultBrowser() {
  // As above, we want to use the real main bundle.
  NSString* myIdentifier = [[NSBundle mainBundle] bundleIdentifier];
  if (!myIdentifier)
    return UNKNOWN_DEFAULT_BROWSER;
  return IsIdentifierDefaultBrowser(myIdentifier) ? IS_DEFAULT_BROWSER
                                                  : NOT_DEFAULT_BROWSER;
}

// Returns true if Firefox is the default browser for the current user.
bool ShellIntegration::IsFirefoxDefaultBrowser() {
  return IsIdentifierDefaultBrowser(@"org.mozilla.firefox");
}
