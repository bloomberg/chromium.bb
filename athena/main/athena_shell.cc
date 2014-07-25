// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/test/athena_test_helper.h"
#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/gl/gl_surface.h"

class UIShell {
 public:
  explicit UIShell(base::MessageLoopForUI* message_loop)
      : athena_helper_(message_loop) {
    const bool enable_pixel_output = true;
    ui::ContextFactory* factory =
        ui::InitializeContextFactoryForTests(enable_pixel_output);
    athena_helper_.SetUp(factory);
    athena_helper_.host()->Show();

    InitSampleActivities();
  }

  void InitSampleActivities() {
    athena::ActivityManager::Get()->AddActivity(
        athena::ActivityFactory::Get()->CreateWebActivity(
            NULL, GURL("http://www.google.com/")));
  }

 private:
  athena::test::AthenaTestHelper athena_helper_;

  DISALLOW_COPY_AND_ASSIGN(UIShell);
};

int main(int argc, const char **argv) {
  setlocale(LC_ALL, "");

  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::i18n::InitializeICU();
  gfx::GLSurface::InitializeOneOffForTests();

  base::FilePath test_pak_path;
  CHECK(PathService::Get(base::DIR_MODULE, &test_pak_path));
  test_pak_path = test_pak_path.AppendASCII("athena_resources.pak");
  ui::ResourceBundle::InitSharedInstanceWithPakPath(test_pak_path);

  base::MessageLoopForUI message_loop;
  UIShell shell(&message_loop);
  base::RunLoop run_loop;
  run_loop.Run();
}
