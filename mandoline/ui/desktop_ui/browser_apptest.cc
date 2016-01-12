// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "mandoline/ui/desktop_ui/public/interfaces/launch_handler.mojom.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/application_test_base.h"

namespace mandoline {

class BrowserTest : public mojo::test::ApplicationTestBase {
 public:
  BrowserTest() : ApplicationTestBase() {}
  ~BrowserTest() override {}

  // mojo::test::ApplicationTestBase:
  void SetUp() override {
    mojo::test::ApplicationTestBase::SetUp();
    application_impl()->ConnectToService("mojo:desktop_ui", &launch_handler_);
    ASSERT_TRUE(launch_handler_.is_bound());
  }

 protected:
  LaunchHandlerPtr launch_handler_;

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(BrowserTest);
};

// A simple sanity check for connecting to the LaunchHandler.
TEST_F(BrowserTest, LaunchHandlerBasic) {
  ASSERT_TRUE(launch_handler_.is_bound());
  launch_handler_->LaunchURL(mojo::String::From("data:text/html,foo"));
  EXPECT_TRUE(launch_handler_.is_bound());
}

}  // namespace mandoline