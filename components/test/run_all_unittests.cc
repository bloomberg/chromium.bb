// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "content/public/test/test_content_client_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace components {

class ComponentsUnitTestEventListener : public testing::EmptyTestEventListener {
 public:
  ComponentsUnitTestEventListener() {}
  virtual ~ComponentsUnitTestEventListener() {}

  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    content_initializer_.reset(new content::TestContentClientInitializer());
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) OVERRIDE {
    content_initializer_.reset();
  }

 private:
  scoped_ptr<content::TestContentClientInitializer> content_initializer_;

  DISALLOW_COPY_AND_ASSIGN(ComponentsUnitTestEventListener);
};

}  // namespace components

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  // The listener will set up common test environment for all components unit
  // tests.
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new components::ComponentsUnitTestEventListener());

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&base::TestSuite::Run,
                             base::Unretained(&test_suite)));
}
