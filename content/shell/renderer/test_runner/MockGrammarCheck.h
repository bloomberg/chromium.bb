// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockGrammarCheck_h
#define MockGrammarCheck_h

#include <vector>

namespace blink {

class WebString;
struct WebTextCheckingResult;

}

namespace WebTestRunner {

// A mock implementation of a grammar-checker used for WebKit tests. This class
// only implements the minimal functionarities required by WebKit tests, i.e.
// this class just compares the given string with known grammar mistakes in
// webkit tests and adds grammar markers on them. Even though this is sufficent
// for webkit tests, this class is not suitable for any other usages.
class MockGrammarCheck {
public:
    static bool checkGrammarOfString(const blink::WebString&, std::vector<blink::WebTextCheckingResult>*);
};

}

#endif // MockSpellCheck_h
