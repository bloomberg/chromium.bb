// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/dom_ui/advanced_options_utils_mac.h"
#include "base/logging.h"

void AdvancedOptionsUtilities::ShowNetworkProxySettings() {
  NSArray* itemsToOpen = [NSArray arrayWithObject:[NSURL fileURLWithPath:
      @"/System/Library/PreferencePanes/Network.prefPane"]];

  const char* proxyPrefCommand = "Proxies";
  AEDesc openParams = { typeNull, NULL };
  OSStatus status = AECreateDesc('ptru',
                                 proxyPrefCommand,
                                 strlen(proxyPrefCommand),
                                 &openParams);
  LOG_IF(ERROR, status != noErr) << "Failed to create open params: " << status;

  LSLaunchURLSpec launchSpec = { 0 };
  launchSpec.itemURLs = (CFArrayRef)itemsToOpen;
  launchSpec.passThruParams = &openParams;
  launchSpec.launchFlags = kLSLaunchAsync | kLSLaunchDontAddToRecents;
  LSOpenFromURLSpec(&launchSpec, NULL);

  if (openParams.descriptorType != typeNull)
    AEDisposeDesc(&openParams);
}

void AdvancedOptionsUtilities::ShowManageSSLCertificates() {
  NSString* const kKeychainBundleId = @"com.apple.keychainaccess";
  [[NSWorkspace sharedWorkspace]
   launchAppWithBundleIdentifier:kKeychainBundleId
   options:0L
   additionalEventParamDescriptor:nil
   launchIdentifier:nil];
}

