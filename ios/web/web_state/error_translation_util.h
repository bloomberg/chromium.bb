// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_ERROR_TRANSLATION_UTIL_H_
#define IOS_WEB_WEB_STATE_ERROR_TRANSLATION_UTIL_H_

#import <Foundation/Foundation.h>

namespace web {

// Translates iOS-specific errors into their net error equivalent.  If a valid
// translation is found, a copy of |error| is returned with the translation as
// its final underlying error.  If there is no viable translation, the original
// error is returned.
NSError* NetErrorFromError(NSError* error);

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_ERROR_TRANSLATION_UTIL_H_
