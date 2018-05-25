// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  mojo::edk::Init();

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
