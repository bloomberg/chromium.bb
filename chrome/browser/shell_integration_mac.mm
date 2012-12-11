// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/foundation_util.h"
#include "chrome/common/chrome_version_info.h"
#import "third_party/mozilla/NSWorkspace+Utils.h"

ShellIntegration::DefaultWebClientSetPermission
    ShellIntegration::CanSetAsDefaultBrowser() {
  if (chrome::VersionInfo::GetChannel() !=
          chrome::VersionInfo::CHANNEL_CANARY) {
    return SET_DEFAULT_UNATTENDED;
  }

  return SET_DEFAULT_NOT_ALLOWED;
}

// Sets Chromium as default browser to be used by the operating system. This
// applies only for the current user. Returns false if this cannot be done, or
// if the operation fails.
bool ShellIntegration::SetAsDefaultBrowser() {
  if (CanSetAsDefaultBrowser() != SET_DEFAULT_UNATTENDED)
    return false;

  // We really do want the outer bundle here, not the main bundle since setting
  // a shortcut to Chrome as the default browser doesn't make sense.
  NSString* identifier = [base::mac::OuterBundle() bundleIdentifier];
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

  if (CanSetAsDefaultProtocolClient() != SET_DEFAULT_UNATTENDED)
    return false;

  // We really do want the main bundle here since it makes sense to set an
  // app shortcut as a default protocol handler.
  NSString* identifier = [base::mac::MainBundle() bundleIdentifier];
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
ShellIntegration::DefaultWebClientState ShellIntegration::GetDefaultBrowser() {
  // We really do want the outer bundle here, since this we want to know the
  // status of the main Chrome bundle and not a shortcut.
  NSString* my_identifier = [base::mac::OuterBundle() bundleIdentifier];
  if (!my_identifier)
    return UNKNOWN_DEFAULT;

  return IsIdentifierDefaultBrowser(my_identifier) ? IS_DEFAULT : NOT_DEFAULT;
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
    return UNKNOWN_DEFAULT;

  // We really do want the main bundle here since it makes sense to set an
  // app shortcut as a default protocol handler.
  NSString* my_identifier = [base::mac::MainBundle() bundleIdentifier];
  if (!my_identifier)
    return UNKNOWN_DEFAULT;

  NSString* protocol_ns = [NSString stringWithUTF8String:protocol.c_str()];
  return IsIdentifierDefaultProtocolClient(my_identifier, protocol_ns) ?
      IS_DEFAULT : NOT_DEFAULT;
}
