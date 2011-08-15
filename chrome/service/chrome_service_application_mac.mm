// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/service/chrome_service_application_mac.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "content/common/cloud_print_class_mac.h"
#include "chrome/common/chrome_switches.h"

@interface ServiceCrApplication ()
- (void)setCloudPrintHandler;
- (void)submitPrint:(NSAppleEventDescriptor*)event;
@end

@implementation ServiceCrApplication

-(void)setCloudPrintHandler {
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em setEventHandler:self
          andSelector:@selector(submitPrint:)
        forEventClass:content::kAECloudPrintClass
           andEventID:content::kAECloudPrintClass];
}

// Event handler for Cloud Print Event. Forwards print job received to Chrome,
// launching Chrome if necessary. Used to beat CUPS sandboxing.
- (void)submitPrint:(NSAppleEventDescriptor*)event {
  std::string silent = std::string("--") + switches::kNoStartupWindow;
  // Set up flag so that it can be passed along with the Apple Event.
  CFStringRef silentLaunchFlag =
      CFStringCreateWithCString(NULL, silent.c_str(), kCFStringEncodingUTF8);
  CFStringRef flags[] = { silentLaunchFlag };
  // Argv array that will be passed.
  CFArrayRef passArgv =
      CFArrayCreate(NULL, (const void**) flags, 1, &kCFTypeArrayCallBacks);
  FSRef ref;
  OSStatus status = noErr;
  CFURLRef* kDontWantURL = NULL;
  // Get Chrome's bundle ID.
  std::string bundleID =  base::mac::BaseBundleID();
  CFStringRef bundleIDCF =
      CFStringCreateWithCString(NULL, bundleID.c_str(), kCFStringEncodingUTF8);
  // Use Launch Services to locate Chrome using its bundleID.
  status = LSFindApplicationForInfo(kLSUnknownCreator, bundleIDCF,
                                    NULL, &ref, kDontWantURL);

  if (status != noErr) {
    LOG(ERROR) << "Failed to make path ref";
    LOG(ERROR) << GetMacOSStatusErrorString(status);
    LOG(ERROR) << GetMacOSStatusCommentString(status);
    return;
  }
  // Actually create the Apple Event.
  NSAppleEventDescriptor* sendEvent =
      [NSAppleEventDescriptor
           appleEventWithEventClass:content::kAECloudPrintClass
                            eventID:content::kAECloudPrintClass
                   targetDescriptor:nil
                           returnID:kAutoGenerateReturnID
                      transactionID:kAnyTransactionID];
  // Pull the parameters out of AppleEvent sent to us and attach them
  // to our Apple Event.
  NSAppleEventDescriptor* parameters =
      [event paramDescriptorForKeyword:content::kAECloudPrintClass];
  [sendEvent setParamDescriptor:parameters
                     forKeyword:content::kAECloudPrintClass];
  LSApplicationParameters params = { 0,
                                     kLSLaunchDefaults,
                                     &ref,
                                     NULL,
                                     NULL,
                                     passArgv,
                                     NULL };
  AEDesc* initialEvent = const_cast<AEDesc*> ([sendEvent aeDesc]);
  params.initialEvent = static_cast<AppleEvent*> (initialEvent);
  // Send the Apple Event Using launch services, launching Chrome if necessary.
  status = LSOpenApplication(&params, NULL);
  if (status != noErr) {
    LOG(ERROR) << "Unable to launch";
    LOG(ERROR) << GetMacOSStatusErrorString(status);
    LOG(ERROR) << GetMacOSStatusCommentString(status);
  }
}


@end


namespace chrome_service_application_mac {

void RegisterServiceCrApp() {
  ServiceCrApplication* var =
      static_cast<ServiceCrApplication*>
          ([ServiceCrApplication sharedApplication]);
  [var setCloudPrintHandler];
}

}  // namespace chrome_service_application_mac

