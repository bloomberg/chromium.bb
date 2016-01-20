// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_CONSTRAINTS_H_
#define COMPONENTS_TEST_RUNNER_MOCK_CONSTRAINTS_H_

namespace blink {
class WebMediaConstraints;
class WebString;
}

namespace test_runner {

class MockConstraints {
 public:
  static bool VerifyConstraints(const blink::WebMediaConstraints& constraints,
                                blink::WebString* failed_constraint = 0);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_CONSTRAINTS_H_
