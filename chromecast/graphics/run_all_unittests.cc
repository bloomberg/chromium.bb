// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

class ChromecastGraphicsTestSuite : public base::TestSuite {
 public:
  ChromecastGraphicsTestSuite(int argc, char** argv)
      : base::TestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();
    gl::GLSurfaceTestSupport::InitializeOneOff();
    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    env_ = aura::Env::CreateInstance();
  }

  void Shutdown() override {
    env_.reset();
    ui::ResourceBundle::CleanupSharedInstance();
    base::TestSuite::Shutdown();
  }

 private:
  std::unique_ptr<aura::Env> env_;
  DISALLOW_COPY_AND_ASSIGN(ChromecastGraphicsTestSuite);
};

int main(int argc, char** argv) {
  ChromecastGraphicsTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(argc, argv,
                               base::Bind(&ChromecastGraphicsTestSuite::Run,
                                          base::Unretained(&test_suite)));
}
