// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ApplicationServices/ApplicationServices.h>
#import <Foundation/NSAutoreleasePool.h>

#include "cloud_print/virtual_driver/posix/installer_util_mac.h"

const AEEventClass  kAECloudPrintInstallClass= 'GCPi';

// Installer for Virtual Driver on Mac. Sends an Apple Event to
// Chrome, launching it if necessary. The Apple Event registers
// the virtual driver with the service process.
int main() {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  cloud_print::sendServiceProcessEvent(kAECloudPrintInstallClass);
  [pool release];
  return 0;
}
