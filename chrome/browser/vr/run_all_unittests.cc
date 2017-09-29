// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace {

class VrCommonTestSuite : public base::TestSuite {
 public:
  VrCommonTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();

    ui::RegisterPathProvider();

    base::FilePath pak_path;
#if defined(OS_ANDROID)
    PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path);
#else
    PathService::Get(base::DIR_MODULE, &pak_path);
#endif
    ui::ResourceBundle::InitSharedInstanceWithPakPath(
        pak_path.AppendASCII("vr_test.pak"));
  }

  void Shutdown() override {
    ui::ResourceBundle::CleanupSharedInstance();
    base::TestSuite::Shutdown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VrCommonTestSuite);
};

}  // namespace

int main(int argc, char** argv) {
  VrCommonTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&VrCommonTestSuite::Run, base::Unretained(&test_suite)));
}
