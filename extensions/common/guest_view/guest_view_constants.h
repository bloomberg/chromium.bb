// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebView API.

#ifndef EXTENSIONS_COMMON_GUEST_VIEW_GUEST_VIEW_CONSTANTS_H_
#define EXTENSIONS_COMMON_GUEST_VIEW_GUEST_VIEW_CONSTANTS_H_

namespace guestview {

// Sizing attributes/parameters.
extern const char kAttributeAutoSize[];
extern const char kAttributeMaxHeight[];
extern const char kAttributeMaxWidth[];
extern const char kAttributeMinHeight[];
extern const char kAttributeMinWidth[];
extern const char kElementWidth[];
extern const char kElementHeight[];
extern const char kElementSizeIsLogical[];

// Events.
extern const char kEventResize[];

// Parameters/properties on events.
extern const char kContentWindowID[];
extern const char kID[];
extern const char kIsTopLevel[];
extern const char kNewWidth[];
extern const char kNewHeight[];
extern const char kOldWidth[];
extern const char kOldHeight[];
extern const char kReason[];
extern const char kUrl[];
extern const char kUserGesture[];

// Initialization parameters.
extern const char kParameterApi[];
extern const char kParameterInstanceId[];

// Other.
extern const char kGuestViewManagerKeyName[];
extern const int kInstanceIDNone;
extern const int kDefaultWidth;
extern const int kDefaultHeight;

}  // namespace guestview

#endif  // EXTENSIONS_COMMON_GUEST_VIEW_GUEST_VIEW_CONSTANTS_H_
