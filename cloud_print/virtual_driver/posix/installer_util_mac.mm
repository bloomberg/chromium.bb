// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/virtual_driver/posix/installer_util_mac.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSAppleEventDescriptor.h>
#import <CoreServices/CoreServices.h>

#include "base/mac/foundation_util.h"

#include <stdlib.h>
#include <string>
#include <iostream>

namespace cloud_print {
// Sends an event of a class Type sendClass to the Chromium
// service process. Used to install and uninstall the Cloud
// Print driver on Mac.
void sendServiceProcessEvent(const AEEventClass sendClass) {
  FSRef ref;
  OSStatus status = noErr;
  CFURLRef* kDontWantURL = NULL;

  std::string bundleID =  base::mac::BaseBundleID();
  CFStringRef bundleIDCF = CFStringCreateWithCString(NULL, bundleID.c_str(),
                                                     kCFStringEncodingUTF8);

  status = LSFindApplicationForInfo(kLSUnknownCreator, bundleIDCF, NULL,
                                    &ref, kDontWantURL);

  if (status != noErr) {
    std::cerr << "Failed to make path ref";
    std::cerr << GetMacOSStatusErrorString(status);
    std::cerr << GetMacOSStatusCommentString(status);
    exit(-1);
  }

  NSAppleEventDescriptor* sendEvent =
      [NSAppleEventDescriptor appleEventWithEventClass:sendClass
                                               eventID:sendClass
                                      targetDescriptor:nil
                                              returnID:kAutoGenerateReturnID
                                         transactionID:kAnyTransactionID];
  if (sendEvent == nil) {
    // Write to system error log using cerr.
    std::cerr << "Unable to create Apple Event";
  }
  LSApplicationParameters params = { 0, kLSLaunchDefaults, &ref, NULL, NULL,
                                     NULL, NULL };
  AEDesc* initialEvent = const_cast<AEDesc*> ([sendEvent aeDesc]);
  params.initialEvent = static_cast<AppleEvent*> (initialEvent);
  status = LSOpenApplication(&params, NULL);

  if (status != noErr) {
    std::cerr << "Unable to launch Chrome to install";
    std::cerr << GetMacOSStatusErrorString(status);
    std::cerr << GetMacOSStatusCommentString(status);
    exit(-1);
  }
}

}   // namespace cloud_print
