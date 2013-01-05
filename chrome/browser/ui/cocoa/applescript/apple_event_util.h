// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_APPLE_EVENT_UTIL_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_APPLE_EVENT_UTIL_H_

#import <Cocoa/Cocoa.h>

namespace base {
class Value;
}

namespace chrome {
namespace mac {

NSAppleEventDescriptor* ValueToAppleEventDescriptor(const base::Value* value);

}  // namespace mac
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_APPLE_EVENT_UTIL_H_
