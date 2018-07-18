// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_suite.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "services/catalog/catalog.h"

namespace {

const base::FilePath::CharType kCatalogFilename[] =
    FILE_PATH_LITERAL("ash_unittests_catalog.json");

}  // namespace

int main(int argc, char** argv) {
  ash::AshTestSuite test_suite(argc, argv);

  // Load a catalog so that these tests can connect to services.
  catalog::Catalog::LoadDefaultCatalogManifest(
      base::FilePath(kCatalogFilename));

  mojo::core::Init();
  // The IPC thread is necessary for the window service.
  base::Thread ipc_thread("IPC thread");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&ash::AshTestSuite::Run, base::Unretained(&test_suite)));
}
