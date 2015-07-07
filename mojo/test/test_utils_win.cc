// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/test/test_utils.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
namespace mojo {
namespace test {

base::FilePath GetFilePathForJSResource(const std::string& path) {
  std::string binding_path = "gen/" + path + ".js";
  base::ReplaceChars(binding_path, "//", "\\", &binding_path);
  base::FilePath exe_dir;
  PathService::Get(base::DIR_EXE, &exe_dir);
  return exe_dir.AppendASCII(binding_path);
}

}  // namespace test
}  // namespace mojo
