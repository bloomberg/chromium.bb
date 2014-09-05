// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_apitest.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

ShellApiTest::ShellApiTest() {
}

ShellApiTest::~ShellApiTest() {
}

bool ShellApiTest::RunAppTest(const std::string& app_dir) {
  base::FilePath test_data_dir;
  PathService::Get(extensions::DIR_TEST_DATA, &test_data_dir);
  test_data_dir = test_data_dir.AppendASCII(app_dir);
  ResultCatcher catcher;

  bool loaded = extension_system_->LoadApp(test_data_dir);
  if (!loaded)
    return false;

  extension_system_->LaunchApp();

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    return false;
  }

  return true;
}

}  // namespace extensions
