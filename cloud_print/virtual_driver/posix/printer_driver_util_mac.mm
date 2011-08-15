// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/virtual_driver/posix/printer_driver_util_posix.h"

#import <ApplicationServices/ApplicationServices.h>
#import <CoreServices/CoreServices.h>
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSAppleEventDescriptor.h>
#import <ScriptingBridge/SBApplication.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"

#include <cups/backend.h>

#include <stdlib.h>
#include <string>

// Duplicated is content/common/cloud_print_class_mac.h
const AEEventClass kAECloudPrintClass = 'GCPp';

namespace cloud_print {
// Checks to see whether the browser process, whose bundle ID
// is specified by bundle ID, is running.
bool IsBrowserRunning(std::string bundleID) {
  SBApplication* app = [SBApplication applicationWithBundleIdentifier:
                           [NSString stringWithUTF8String:bundleID.c_str()]];
  if ([app isRunning]) {
    return true;
  }
  return false;
}
}   // namespace cloud_print

void LaunchPrintDialog(const std::string& outputPath,
                       const std::string& jobTitle,
                       const std::string& user) {
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  OSStatus status = noErr;
  FSRef ref;
  // Get the bundleID of the browser.
  std::string bundleID =  base::mac::BaseBundleID();
  // If the browser is running, send the event to it.
  // Otherwise, send the event to the service process.
  if (!cloud_print::IsBrowserRunning(bundleID)) {
    // Generate the bundle ID for the Service process.
    bundleID = bundleID + ".helper";
  }
  CFStringRef bundleIDCF = CFStringCreateWithCString(
                               NULL,
                               bundleID.c_str(),
                               kCFStringEncodingUTF8);
  CFURLRef* kDontWantURL = NULL;
  // Locate the service process with the help of the bundle ID.
  status = LSFindApplicationForInfo(kLSUnknownCreator, bundleIDCF,
                                    NULL, &ref, kDontWantURL);

  if (status != noErr) {
    LOG(ERROR) << "Couldn't locate the process to send Apple Event";
    exit(CUPS_BACKEND_CANCEL);
  }

  // Create the actual Apple Event.
  NSAppleEventDescriptor* event =
      [NSAppleEventDescriptor appleEventWithEventClass:kAECloudPrintClass
                                               eventID:kAECloudPrintClass
                                      targetDescriptor:nil
                                              returnID:kAutoGenerateReturnID
                                         transactionID:kAnyTransactionID];

  if(event == nil) {
    LOG(ERROR) << "Unable to Create Event";
    exit(CUPS_BACKEND_CANCEL);
  }

  // Create the AppleEvent parameters.
  NSAppleEventDescriptor* printPath =
      [NSAppleEventDescriptor descriptorWithString:
          [NSString stringWithUTF8String:outputPath.c_str()]];
  NSAppleEventDescriptor* title =
      [NSAppleEventDescriptor descriptorWithString:
          [NSString stringWithUTF8String:jobTitle.c_str()]];
  NSAppleEventDescriptor* mime = [NSAppleEventDescriptor
                                  descriptorWithString:@"application/pdf"];

  // Create and populate the list of parameters.
  // Note that the array starts at index 1.
  NSAppleEventDescriptor* parameters = [NSAppleEventDescriptor listDescriptor];

  if(parameters == nil) {
    LOG(ERROR) << "Unable to Create Paramters";
    exit(CUPS_BACKEND_CANCEL);
  }

  [parameters insertDescriptor:mime atIndex:1];
  [parameters insertDescriptor:printPath atIndex:2];
  [parameters insertDescriptor:title atIndex:3];
  [event setParamDescriptor:parameters forKeyword:kAECloudPrintClass];

  // Set the application launch parameters.
  // We are just using launch services to deliver our Apple Event.
  LSApplicationParameters params = {
      0, kLSLaunchDefaults , &ref, NULL, NULL, NULL, NULL };

  AEDesc* initialEvent = const_cast<AEDesc*> ([event aeDesc]);
  params.initialEvent = static_cast<AppleEvent*> (initialEvent);
  // Deliver the Apple Event using launch services.
  status = LSOpenApplication(&params, NULL);
  if (status != noErr) {
    LOG(ERROR) << "Unable to launch";
    LOG(ERROR) << GetMacOSStatusErrorString(status);
    LOG(ERROR) << GetMacOSStatusCommentString(status);
    exit(CUPS_BACKEND_CANCEL);
  }

  [pool release];
  return;
}
