// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi_mac/gcapi.h"

#import <Foundation/Foundation.h>

int main() {
  unsigned reasons;
  int b = GoogleChromeCompatibilityCheck(&reasons);
  NSLog(@"%d: %x", b, reasons);

  LaunchGoogleChrome();
}
