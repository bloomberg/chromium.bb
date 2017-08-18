// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/eme_constants.h"
#include "media/base/media.h"
#include "media/base/pipeline_status.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/test/pipeline_integration_test_base.h"

namespace {

// Limit the amount of initial (or post-seek) audio silence padding allowed in
// rendering of fuzzed input.
constexpr base::TimeDelta kMaxPlayDelay = base::TimeDelta::FromSeconds(10);

void OnEncryptedMediaInitData(media::PipelineIntegrationTestBase* test,
                              media::EmeInitDataType /* type */,
                              const std::vector<uint8_t>& /* init_data */) {
  // Encrypted media is not supported in this test. For an encrypted media file,
  // we will start demuxing the data but media pipeline will wait for a CDM to
  // be available to start initialization, which will not happen in this case.
  // To prevent the test timeout, we'll just fail the test immediately here.
  // TODO(xhwang): Support encrypted media in this fuzzer test.
  test->FailTest(media::PIPELINE_ERROR_INITIALIZATION_FAILED);
}

void OnAudioPlayDelay(media::PipelineIntegrationTestBase* test,
                      base::TimeDelta play_delay) {
  CHECK_GT(play_delay, base::TimeDelta());
  if (play_delay > kMaxPlayDelay)
    test->FailTest(media::PIPELINE_ERROR_INITIALIZATION_FAILED);
}

}  // namespace

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Media pipeline starts new threads, which needs AtExitManager.
  base::AtExitManager at_exit;

  // Media pipeline checks command line arguments internally.
  base::CommandLine::Init(0, nullptr);

  media::InitializeMediaLibrary();

  media::PipelineIntegrationTestBase test;

  test.set_encrypted_media_init_data_cb(
      base::Bind(&OnEncryptedMediaInitData, &test));

  test.set_audio_play_delay_cb(
      media::BindToCurrentLoop(base::BindRepeating(&OnAudioPlayDelay, &test)));

  media::PipelineStatus pipeline_status =
      test.Start(data, size,
                 media::PipelineIntegrationTestBase::kClockless |
                     media::PipelineIntegrationTestBase::kUnreliableDuration);
  if (pipeline_status != media::PIPELINE_OK)
    return 0;

  test.Play();
  pipeline_status = test.WaitUntilEndedOrError();
  if (pipeline_status != media::PIPELINE_OK)
    return 0;

  test.Seek(base::TimeDelta());

  return 0;
}
