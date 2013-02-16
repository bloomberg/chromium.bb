// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_POLICY_TEST_UTILS_H_

namespace policy {

class PolicyService;

// Returns true if |service| is not serving any policies. Otherwise logs the
// current policies and returns false.
bool PolicyServiceIsEmpty(const PolicyService* service);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_TEST_UTILS_H_
