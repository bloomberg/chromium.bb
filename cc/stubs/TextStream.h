// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_TEXTSTREAM_H_
#define CC_STUBS_TEXTSTREAM_H_

#include <wtf/text/WTFString.h>

namespace WebCore {

class TextStream {
public:
    TextStream& operator<<(const String&) { return *this; }
    TextStream& operator<<(const char*) { return *this; }
    TextStream& operator<<(int) { return *this; }
    String release() { return ""; }
};

}

#endif  // CC_STUBS_TEXTSTREAM_H_

