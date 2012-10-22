// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_NSCODER_UTIL_H_
#define CHROME_COMMON_MAC_NSCODER_UTIL_H_

#import <Foundation/Foundation.h>

#include <string>

namespace nscoder_util {

// Archives a std::string in an Objective-C key archiver.
void EncodeString(NSCoder* coder, NSString* key, const std::string& string);

// Decode a std::string from an Objective-C key unarchiver.
std::string DecodeString(NSCoder* decoder, NSString* key);

}  // namespace nscoder_util

#endif  // CHROME_COMMON_MAC_NSCODER_UTIL_H_
