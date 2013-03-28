// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/service/chrome_service_application_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/common/cloud_print/cloud_print_class_mac.h"
#include "chrome/common/chrome_switches.h"

@interface ServiceApplication ()
- (void)setCloudPrintHandler;
- (void)submitPrint:(NSAppleEventDescriptor*)event;
@end

@implementation ServiceApplication

- (void)setCloudPrintHandler {
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em setEventHandler:self
          andSelector:@selector(submitPrint:)
        forEventClass:cloud_print::kAECloudPrintClass
           andEventID:cloud_print::kAECloudPrintClass];
}

// Event handler for Cloud Print Event. Forwards print job received to Chrome,
// launching Chrome if necessary. Used to beat CUPS sandboxing.
- (void)submitPrint:(NSAppleEventDescriptor*)event {
  std::string silent = std::string("--") + switches::kNoStartupWindow;
  // Set up flag so that it can be passed along with the Apple Event.
  base::mac::ScopedCFTypeRef<CFStringRef> silentLaunchFlag(
      base::SysUTF8ToCFStringRef(silent));
  CFStringRef flags[] = { silentLaunchFlag };
  // Argv array that will be passed.
  base::mac::ScopedCFTypeRef<CFArrayRef> passArgv(
      CFArrayCreate(NULL, (const void**) flags, 1, &kCFTypeArrayCallBacks));
  FSRef ref;
  // Get Chrome's bundle ID.
  std::string bundleID = base::mac::BaseBundleID();
  base::mac::ScopedCFTypeRef<CFStringRef> bundleIDCF(
      base::SysUTF8ToCFStringRef(bundleID));
  // Use Launch Services to locate Chrome using its bundleID.
  OSStatus status = LSFindApplicationForInfo(kLSUnknownCreator, bundleIDCF,
                                             NULL, &ref, NULL);

  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "Failed to make path ref";
    return;
  }
  // Actually create the Apple Event.
  NSAppleEventDescriptor* sendEvent =
      [NSAppleEventDescriptor
           appleEventWithEventClass:cloud_print::kAECloudPrintClass
                            eventID:cloud_print::kAECloudPrintClass
                   targetDescriptor:nil
                           returnID:kAutoGenerateReturnID
                      transactionID:kAnyTransactionID];
  // Pull the parameters out of AppleEvent sent to us and attach them
  // to our Apple Event.
  NSAppleEventDescriptor* parameters =
      [event paramDescriptorForKeyword:cloud_print::kAECloudPrintClass];
  [sendEvent setParamDescriptor:parameters
                     forKeyword:cloud_print::kAECloudPrintClass];
  LSApplicationParameters params = { 0,
                                     kLSLaunchDefaults,
                                     &ref,
                                     NULL,
                                     NULL,
                                     passArgv,
                                     NULL };
  AEDesc* initialEvent = const_cast<AEDesc*>([sendEvent aeDesc]);
  params.initialEvent = static_cast<AppleEvent*>(initialEvent);
  // Send the Apple Event Using launch services, launching Chrome if necessary.
  status = LSOpenApplication(&params, NULL);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "Unable to launch";
  }
}


@end


namespace chrome_service_application_mac {

void RegisterServiceApp() {
  ServiceApplication* var =
      base::mac::ObjCCastStrict<ServiceApplication>(
          [ServiceApplication sharedApplication]);
  [var setCloudPrintHandler];
}

}  // namespace chrome_service_application_mac
