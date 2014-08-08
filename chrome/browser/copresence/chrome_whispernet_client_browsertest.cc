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
#include "chrome/browser/extensions/api/copresence/copresence_api.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "media/base/audio_bus.h"

namespace {

copresence::WhispernetClient* GetWhispernetClient(
    content::BrowserContext* context) {
  extensions::CopresenceService* service =
      extensions::CopresenceService::GetFactoryInstance()->Get(context);
  return service ? service->whispernet_client() : NULL;
}

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
  ChromeWhispernetClientTest() : context_(NULL), initialized_(false) {}

  virtual ~ChromeWhispernetClientTest() {}

  void InitializeWhispernet() {
    context_ = browser()->profile();
    run_loop_.reset(new base::RunLoop());
    GetWhispernetClient(context_)->Initialize(base::Bind(
        &ChromeWhispernetClientTest::InitCallback, base::Unretained(this)));
    run_loop_->Run();

    EXPECT_TRUE(initialized_);
  }

  void EncodeTokenAndSaveSamples() {
    copresence::WhispernetClient* client = GetWhispernetClient(context_);
    ASSERT_TRUE(client);

    // This is the base64 encoding for "000000".
    const std::string kZeroToken = "MDAwMDAw";

    run_loop_.reset(new base::RunLoop());
    client->RegisterSamplesCallback(base::Bind(
        &ChromeWhispernetClientTest::SamplesCallback, base::Unretained(this)));
    expected_token_ = kZeroToken;

    client->EncodeToken(kZeroToken);
    run_loop_->Run();

    EXPECT_GT(saved_samples_->frames(), 0);
  }

  void DecodeSamplesAndVerifyToken() {
    copresence::WhispernetClient* client = GetWhispernetClient(context_);
    ASSERT_TRUE(client);

    const std::string kZeroToken = "MDAwMDAw";

    run_loop_.reset(new base::RunLoop());
    client->RegisterTokensCallback(base::Bind(
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

    client->DecodeSamples(AudioBusToString(samples_bus));
    run_loop_->Run();
  }

  void DetectBroadcast() {
    copresence::WhispernetClient* client = GetWhispernetClient(context_);
    ASSERT_TRUE(client);

    run_loop_.reset(new base::RunLoop());
    client->RegisterDetectBroadcastCallback(
        base::Bind(&ChromeWhispernetClientTest::DetectBroadcastCallback,
                   base::Unretained(this)));
    client->DetectBroadcast();
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
  content::BrowserContext* context_;

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
