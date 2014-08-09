// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebView API.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_CONSTANTS_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_CONSTANTS_H_

namespace guestview {

// Parameters/properties on events.
extern const char kIsTopLevel[];
extern const char kReason[];
extern const char kUrl[];
extern const char kUserGesture[];

// Initialization parameters.
extern const char kParameterApi[];
extern const char kParameterInstanceId[];

// Other.
extern const char kGuestViewManagerKeyName[];
extern const int kInstanceIDNone;

}  // namespace guestview

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_CONSTANTS_H_

