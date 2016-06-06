// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/test/mash_test_suite.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace mash {
namespace test {

MashTestSuite::MashTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}

MashTestSuite::~MashTestSuite() {}

void MashTestSuite::Initialize() {
  base::TestSuite::Initialize();

  // Load ash resources and en-US strings; not 'common' (Chrome) resources.
  // TODO(msw): Check ResourceBundle::IsScaleFactorSupported; load 300% etc.
  base::FilePath path;
  PathService::Get(base::DIR_MODULE, &path);
  base::FilePath mash_test_strings =
      path.Append(FILE_PATH_LITERAL("ash_mus_resources.pak"));

  ui::ResourceBundle::InitSharedInstanceWithPakPath(mash_test_strings);

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
  env_ = aura::Env::CreateInstance();
}

void MashTestSuite::Shutdown() {
  env_.reset();
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace mash
