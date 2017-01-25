// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "build/build_config.h"
#include "content/public/test/unittest_test_suite.h"
#include "content/test/content_test_suite.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"
#include "services/catalog/catalog.h"

#if !defined(OS_ANDROID)

namespace {

const base::FilePath::CharType kCatalogFilename[] =
    FILE_PATH_LITERAL("content_unittests_catalog.json");

}  // namespace

#endif

int main(int argc, char** argv) {
  content::UnitTestTestSuite test_suite(
      new content::ContentTestSuite(argc, argv));

#if !defined(OS_ANDROID)
  catalog::Catalog::LoadDefaultCatalogManifest(
      base::FilePath(kCatalogFilename));
#endif

  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  mojo::edk::ScopedIPCSupport ipc_support(
      test_io_thread.task_runner(),
      mojo::edk::ScopedIPCSupport::ShutdownPolicy::FAST);
  return base::LaunchUnitTests(
      argc, argv, base::Bind(&content::UnitTestTestSuite::Run,
                             base::Unretained(&test_suite)));
}
