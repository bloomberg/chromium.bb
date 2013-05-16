// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_BROWSER_TEST_SUITE_H_
#define CONTENT_TEST_CONTENT_BROWSER_TEST_SUITE_H_

#include "content/public/test/content_test_suite_base.h"

namespace content {

class ContentBrowserTestSuite : public ContentTestSuiteBase {
 public:
  ContentBrowserTestSuite(int argc, char** argv);
  virtual ~ContentBrowserTestSuite();

 protected:
  virtual void Initialize() OVERRIDE;
  virtual ContentClient* CreateClientForInitialization() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ContentBrowserTestSuite);
};

}  // namespace content

#endif  // CONTENT_TEST_CONTENT_BROWSER_TEST_SUITE_H_
