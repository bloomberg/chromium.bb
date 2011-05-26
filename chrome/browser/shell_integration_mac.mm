// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/mac/mac_util.h"
#include "base/mac/foundation_util.h"
#include "chrome/browser/platform_util.h"
#import "third_party/mozilla/NSWorkspace+Utils.h"

// Sets Chromium as default browser to be used by the operating system. This
// applies only for the current user. Returns false if this cannot be done, or
// if the operation fails.
bool ShellIntegration::SetAsDefaultBrowser() {
  if (!platform_util::CanSetAsDefaultBrowser())
    return false;

  // We really do want the main bundle here, not base::mac::MainAppBundle(),
  // which is the bundle for the framework.
  NSString* identifier = [[NSBundle mainBundle] bundleIdentifier];
  if (!identifier)
    return false;

  [[NSWorkspace sharedWorkspace] setDefaultBrowserWithIdentifier:identifier];
  return true;
}

// Sets Chromium as the default application to be used by the operating system
// for the given protocol. This applies only for the current user. Returns false
// if this cannot be done, or if the operation fails.
bool ShellIntegration::SetAsDefaultProtocolClient(const std::string& protocol) {
  if (protocol.empty())
    return false;

  if (!platform_util::CanSetAsDefaultProtocolClient(protocol))
    return false;

  // We really do want the main bundle here, not base::mac::MainAppBundle(),
  // which is the bundle for the framework.
  NSString* identifier = [[NSBundle mainBundle] bundleIdentifier];
  if (!identifier)
    return false;

  NSString* protocol_ns = [NSString stringWithUTF8String:protocol.c_str()];
  OSStatus return_code =
      LSSetDefaultHandlerForURLScheme(base::mac::NSToCFCast(protocol_ns),
                                      base::mac::NSToCFCast(identifier));
  return return_code == noErr;
}

namespace {

// Returns true if |identifier| is the bundle id of the default browser.
bool IsIdentifierDefaultBrowser(NSString* identifier) {
  NSString* default_browser =
      [[NSWorkspace sharedWorkspace] defaultBrowserIdentifier];
  if (!default_browser)
    return false;

  // We need to ensure we do the comparison case-insensitive as LS doesn't
  // persist the case of our bundle id.
  NSComparisonResult result =
      [default_browser caseInsensitiveCompare:identifier];
  return result == NSOrderedSame;
}

// Returns true if |identifier| is the bundle id of the default client
// application for the given protocol.
bool IsIdentifierDefaultProtocolClient(NSString* identifier,
                                       NSString* protocol) {
  CFStringRef default_client_cf =
      LSCopyDefaultHandlerForURLScheme(base::mac::NSToCFCast(protocol));
  NSString* default_client = static_cast<NSString*>(
      base::mac::CFTypeRefToNSObjectAutorelease(default_client_cf));
  if (!default_client)
    return false;

  // We need to ensure we do the comparison case-insensitive as LS doesn't
  // persist the case of our bundle id.
  NSComparisonResult result =
      [default_client caseInsensitiveCompare:identifier];
  return result == NSOrderedSame;
}

}  // namespace

// Attempt to determine if this instance of Chrome is the default browser and
// return the appropriate state. (Defined as being the handler for HTTP/HTTPS
// protocols; we don't want to report "no" here if the user has simply chosen
// to open HTML files in a text editor and FTP links with an FTP client.)
ShellIntegration::DefaultWebClientState ShellIntegration::IsDefaultBrowser() {
  // We really do want the main bundle here, not base::mac::MainAppBundle(),
  // which is the bundle for the framework.
  NSString* my_identifier = [[NSBundle mainBundle] bundleIdentifier];
  if (!my_identifier)
    return UNKNOWN_DEFAULT_WEB_CLIENT;

  return IsIdentifierDefaultBrowser(my_identifier) ? IS_DEFAULT_WEB_CLIENT
                                                   : NOT_DEFAULT_WEB_CLIENT;
}

// Returns true if Firefox is the default browser for the current user.
bool ShellIntegration::IsFirefoxDefaultBrowser() {
  return IsIdentifierDefaultBrowser(@"org.mozilla.firefox");
}

// Attempt to determine if this instance of Chrome is the default client
// application for the given protocol and return the appropriate state.
ShellIntegration::DefaultWebClientState
    ShellIntegration::IsDefaultProtocolClient(const std::string& protocol) {
  if (protocol.empty())
    return UNKNOWN_DEFAULT_WEB_CLIENT;

  // We really do want the main bundle here, not base::mac::MainAppBundle(),
  // which is the bundle for the framework.
  NSString* my_identifier = [[NSBundle mainBundle] bundleIdentifier];
  if (!my_identifier)
    return UNKNOWN_DEFAULT_WEB_CLIENT;

  NSString* protocol_ns = [NSString stringWithUTF8String:protocol.c_str()];
  return IsIdentifierDefaultProtocolClient(my_identifier, protocol_ns) ?
      IS_DEFAULT_WEB_CLIENT : NOT_DEFAULT_WEB_CLIENT;
}
