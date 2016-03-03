// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "media/base/pipeline_status.h"
#include "media/test/pipeline_integration_test_base.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Media pipeline starts new threads, which needs AtExitManager.
  base::AtExitManager at_exit;

  // Media pipeline checks command line arguments internally.
  base::CommandLine::Init(0, nullptr);

  media::PipelineIntegrationTestBase test;

  media::PipelineStatus pipeline_status =
      test.Start(data, size, media::PipelineIntegrationTestBase::kClockless);
  if (pipeline_status != media::PIPELINE_OK)
    return 0;

  test.Play();
  pipeline_status = test.WaitUntilEndedOrError();
  if (pipeline_status != media::PIPELINE_OK)
    return 0;

  test.Seek(base::TimeDelta());

  return 0;
}
