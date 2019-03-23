// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/not_implemented_api_bindings.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

namespace {
constexpr char kStubBindingsPath[] =
    FILE_PATH_LITERAL("fuchsia/runners/cast/not_implemented_api_bindings.js");
}  // namespace

void InjectNotImplementedApiBindings(chromium::web::Frame* frame) {
  base::FilePath stub_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &stub_path));
  stub_path = stub_path.AppendASCII(kStubBindingsPath);
  DCHECK(base::PathExists(stub_path));

  fuchsia::mem::Buffer stub_buf = cr_fuchsia::MemBufferFromFile(
      base::File(stub_path, base::File::FLAG_OPEN | base::File::FLAG_READ));
  CHECK(stub_buf.vmo);

  frame->ExecuteJavaScript(
      {"*"}, std::move(stub_buf), chromium::web::ExecuteMode::ON_PAGE_LOAD,
      [](bool result) { CHECK(result) << "Couldn't inject stub bindings."; });
}
