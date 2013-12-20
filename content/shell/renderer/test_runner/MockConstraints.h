// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockConstraints_h
#define MockConstraints_h

namespace blink {
class WebMediaConstraints;
class WebString;
}

namespace WebTestRunner {

class MockConstraints {
public:
    static bool verifyConstraints(const blink::WebMediaConstraints&, blink::WebString* failedConstraint = 0);
};

}

#endif // MockConstraints_h
