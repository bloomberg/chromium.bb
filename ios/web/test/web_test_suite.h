// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_WEB_TEST_SUITE_H_
#define IOS_WEB_TEST_WEB_TEST_SUITE_H_

#include "base/test/test_suite.h"

namespace web {

class WebTestSuite : public base::TestSuite {
 public:
  WebTestSuite(int argc, char** argv);
  ~WebTestSuite() override;

 protected:
  void Initialize() override;

 private:

  DISALLOW_COPY_AND_ASSIGN(WebTestSuite);
};

}  // namespace web

#endif  // IOS_WEB_TEST_WEB_TEST_SUITE_H_
