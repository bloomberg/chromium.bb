// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_TEXTSTREAM_H_
#define CC_STUBS_TEXTSTREAM_H_

#include <wtf/text/WTFString.h>

namespace WebCore {

// These symbols are all defined inside WebCore.
class TextStream {
public:
    TextStream& operator<<(const String&);
    TextStream& operator<<(const char*);
    TextStream& operator<<(int);
    String release();
};

}

#endif  // CC_STUBS_TEXTSTREAM_H_

