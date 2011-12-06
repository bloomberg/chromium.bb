// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CHROME_SERVICE_APPLICATION_MAC_H_
#define CHROME_SERVICE_CHROME_SERVICE_APPLICATION_MAC_H_
#pragma once

#ifdef __OBJC__

#import <AppKit/AppKit.h>

// Top level Mac Application for the service process.
@interface ServiceApplication : NSApplication

@end

#endif  // __OBJC__

namespace chrome_service_application_mac {

// To be used to instantiate ServiceApplication from C++ code.
void RegisterServiceApp();

}  // namespace chrome_service_application_mac

#endif  // CHROME_SERVICE_CHROME_SERVICE_APPLICATION_MAC_H_
