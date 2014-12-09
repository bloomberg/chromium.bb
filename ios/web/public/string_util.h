// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_STRING_UTIL_H_
#define IOS_WEB_PUBLIC_STRING_UTIL_H_

#include "base/strings/string16.h"

namespace web {

// Truncates |contents| to |length|.
// Returns a string terminated at the last space to ensure no words are
// clipped.
// Note: This function uses spaces as word boundaries and may not handle all
// languages correctly.
base::string16 GetStringByClippingLastWord(const base::string16& contents,
                                           size_t length);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_STRING_UTIL_H_
