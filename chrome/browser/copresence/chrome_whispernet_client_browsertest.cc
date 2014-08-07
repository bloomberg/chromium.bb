// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/copresence/chrome_whispernet_client.h"

#include <cstdlib>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/api/copresence_private/copresence_private_api.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "media/base/audio_bus.h"

namespace {

// Copied from src/components/copresence/mediums/audio/audio_recorder.cc
std::string AudioBusToString(scoped_refptr<media::AudioBusRefCounted> source) {
  std::string buffer;
  buffer.resize(source->frames() * source->channels() * sizeof(float));
  float* buffer_view = reinterpret_cast<float*>(string_as_array(&buffer));

  const int channels = source->channels();
  for (int ch = 0; ch < channels; ++ch) {
    for (int si = 0, di = ch; si < source->frames(); ++si, di += channels)
      buffer_view[di] = source->channel(ch)[si];
  }

  return buffer;
}

}  // namespace

class ChromeWhispernetClientTest : public ExtensionBrowserTest {
 public:
  ChromeWhispernetClientTest() : initialized_(false) {}

  virtual ~ChromeWhispernetClientTest() {
    if (client_)
      extensions::SetWhispernetClientForTesting(NULL);
  }

  void InitializeWhispernet() {
    run_loop_.reset(new base::RunLoop());
    client_.reset(new ChromeWhispernetClient(browser()->profile()));
    extensions::SetWhispernetClientForTesting(client_.get());

    client_->Initialize(base::Bind(&ChromeWhispernetClientTest::InitCallback,
                                   base::Unretained(this)));
    run_loop_->Run();

    EXPECT_TRUE(initialized_);
  }

  void EncodeTokenAndSaveSamples() {
    ASSERT_TRUE(client_);

    // This is the base64 encoding for 000000.
    const std::string kZeroToken = "MDAwMDAw";

    run_loop_.reset(new base::RunLoop());
    client_->RegisterSamplesCallback(base::Bind(
        &ChromeWhispernetClientTest::SamplesCallback, base::Unretained(this)));
    expected_token_ = kZeroToken;

    client_->EncodeToken(kZeroToken);
    run_loop_->Run();

    EXPECT_GT(saved_samples_->frames(), 0);
  }

  void DecodeSamplesAndVerifyToken() {
    ASSERT_TRUE(client_);

    const std::string kZeroToken = "MDAwMDAw";

    run_loop_.reset(new base::RunLoop());
    client_->RegisterTokensCallback(base::Bind(
        &ChromeWhispernetClientTest::TokensCallback, base::Unretained(this)));
    expected_token_ = kZeroToken;

    ASSERT_GT(saved_samples_->frames(), 0);

    // Convert our single channel samples to two channel. Decode samples
    // expects 2 channel data.
    scoped_refptr<media::AudioBusRefCounted> samples_bus =
        media::AudioBusRefCounted::Create(2, saved_samples_->frames());
    memcpy(samples_bus->channel(0),
           saved_samples_->channel(0),
           sizeof(float) * saved_samples_->frames());
    memcpy(samples_bus->channel(1),
           saved_samples_->channel(0),
           sizeof(float) * saved_samples_->frames());

    client_->DecodeSamples(AudioBusToString(samples_bus));
    run_loop_->Run();
  }

  void DetectBroadcast() {
    ASSERT_TRUE(client_);

    run_loop_.reset(new base::RunLoop());
    client_->RegisterDetectBroadcastCallback(
        base::Bind(&ChromeWhispernetClientTest::DetectBroadcastCallback,
                   base::Unretained(this)));
    client_->DetectBroadcast();
    run_loop_->Run();
  }

 protected:
  void InitCallback(bool success) {
    EXPECT_TRUE(success);
    initialized_ = true;
    ASSERT_TRUE(run_loop_);
    run_loop_->Quit();
  }

  void SamplesCallback(
      const std::string& token,
      const scoped_refptr<media::AudioBusRefCounted>& samples) {
    EXPECT_EQ(expected_token_, token);
    saved_samples_ = samples;
    ASSERT_TRUE(run_loop_);
    run_loop_->Quit();
  }

  void TokensCallback(const std::vector<std::string>& tokens) {
    ASSERT_TRUE(run_loop_);
    run_loop_->Quit();

    EXPECT_EQ(expected_token_, tokens[0]);
  }

  void DetectBroadcastCallback(bool success) {
    EXPECT_TRUE(success);
    ASSERT_TRUE(run_loop_);
    run_loop_->Quit();
  }

 private:
  scoped_ptr<base::RunLoop> run_loop_;
  scoped_ptr<ChromeWhispernetClient> client_;

  std::string expected_token_;
  scoped_refptr<media::AudioBusRefCounted> saved_samples_;

  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWhispernetClientTest);
};

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, Initialize) {
  InitializeWhispernet();
}

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, EncodeToken) {
  InitializeWhispernet();
  EncodeTokenAndSaveSamples();
}

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, DecodeSamples) {
  InitializeWhispernet();
  EncodeTokenAndSaveSamples();
  DecodeSamplesAndVerifyToken();
}

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, DetectBroadcast) {
  InitializeWhispernet();
  EncodeTokenAndSaveSamples();
  DecodeSamplesAndVerifyToken();
  DetectBroadcast();
}
