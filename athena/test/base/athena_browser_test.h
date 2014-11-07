// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_BASE_ATHENA_BROWSER_TEST_H_
#define ATHENA_TEST_BASE_ATHENA_BROWSER_TEST_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"

namespace base {
class CommandLine;
}

namespace content {
class BrowserContext;
}

namespace athena {
namespace test {

// Base class for athena browser tests.
class AthenaBrowserTest : public content::BrowserTestBase {
 public:
  AthenaBrowserTest();
  ~AthenaBrowserTest() override;

 protected:
  // Returns the browser context used by the test.
  content::BrowserContext* GetBrowserContext();

 private:
  // content::BrowserTestBase implementation.
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void RunTestOnMainThreadLoop() override;

  DISALLOW_COPY_AND_ASSIGN(AthenaBrowserTest);
};

}  // namespace test
}  // namespace athena

#endif  // ATHENA_TEST_BASE_ATHENA_BROWSER_TEST_H_
