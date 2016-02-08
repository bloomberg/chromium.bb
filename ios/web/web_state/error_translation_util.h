// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_ERROR_TRANSLATION_UTIL_H_
#define IOS_WEB_WEB_STATE_ERROR_TRANSLATION_UTIL_H_

#import <Foundation/Foundation.h>

namespace web {

// Translates an iOS-specific error into its net error equivalent and returns
// a copy of |error| with the translation as its final underlying error.  The
// underlying net error will have an error code of net::ERR_FAILED if no
// specific translation of the iOS error is found.
NSError* NetErrorFromError(NSError* error);

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_ERROR_TRANSLATION_UTIL_H_
