// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/copresence/chrome_whispernet_client.h"

#include <stddef.h>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/copresence/copresence_api.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/audio_modem/public/audio_modem_types.h"
#include "components/audio_modem/public/whispernet_client.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_converter.h"

using audio_modem::WhispernetClient;
using audio_modem::AUDIBLE;
using audio_modem::INAUDIBLE;
using audio_modem::TokenParameters;

namespace {

// TODO(ckehoe): Use randomly generated tokens instead.
const char kSixZeros[] = "MDAwMDAw";
const char kEightZeros[] = "MDAwMDAwMDA";
const char kNineZeros[] = "MDAwMDAwMDAw";

const size_t kTokenLengths[2] = {6, 6};

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

void GetTokenParamsForLengths(const size_t token_lengths[2],
                              TokenParameters* params) {
  params[0].length = token_lengths[0];
  params[1].length = token_lengths[1];
}

void IgnoreResult(bool success) {}

}  // namespace

class ChromeWhispernetClientTest : public ExtensionBrowserTest,
                                   public media::AudioConverter::InputCallback {
 protected:
  ChromeWhispernetClientTest()
      : initialized_(false),
        expected_audible_(false),
        saved_samples_index_(0) {}

  ~ChromeWhispernetClientTest() override {}

  void InitializeWhispernet() {
    std::unique_ptr<WhispernetClient> client(
        new ChromeWhispernetClient(browser()->profile()));
    client->Initialize(base::Bind(
        &ChromeWhispernetClientTest::InitCallback, base::Unretained(this)));

    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
    EXPECT_TRUE(initialized_);
  }

  // This needs to be called before any of the decoder tests are run. We can't
  // run this code in the constructor or the SetUp methods because the audio
  // manager seems to get initialized only *after* ExtensionBrowserTest::SetUp
  // has finished executing. Setting up a testing AudioMager causes the actual
  // create happening later in the browser initialization to fail. The only way
  // around this at the moment seems to be to have this method called from
  // every test before they try to decode.
  void SetupDecode() {
    // We get default parameters here instead of the constructor since
    // initializing Whispernet also creates our AudioManager. Initializing from
    // the test instead causes issues.
    default_params_ = media::AudioManager::Get()->GetInputStreamParameters(
        media::AudioManagerBase::kDefaultDeviceId);

    coder_params_ = media::AudioParameters(
        default_params_.format(), audio_modem::kDefaultChannelLayout,
        audio_modem::kDefaultSampleRate, audio_modem::kDefaultBitsPerSample,
        default_params_.frames_per_buffer());

    converter_.reset(new media::AudioConverter(
        coder_params_, default_params_,
        default_params_.sample_rate() == coder_params_.sample_rate()));
    converter_->AddInput(this);
  }

  void EncodeTokenAndSaveSamples(WhispernetClient* client,
                                 bool audible,
                                 const std::string& token,
                                 const TokenParameters token_params[2]) {
    run_loop_.reset(new base::RunLoop());
    client->RegisterSamplesCallback(
        base::Bind(&ChromeWhispernetClientTest::SamplesCallback,
                   base::Unretained(this)));
    expected_token_ = token;
    expected_audible_ = audible;

    client->EncodeToken(token, audible ? AUDIBLE : INAUDIBLE, token_params);
    run_loop_->Run();

    EXPECT_GT(saved_samples_->frames(), 0);
  }

  void DecodeSamplesAndVerifyToken(WhispernetClient* client,
                                   bool expect_audible,
                                   const std::string& expected_token,
                                   const TokenParameters token_params[2]) {
    run_loop_.reset(new base::RunLoop());
    client->RegisterTokensCallback(base::Bind(
        &ChromeWhispernetClientTest::TokensCallback, base::Unretained(this)));
    expected_token_ = expected_token;
    expected_audible_ = expect_audible;

    ASSERT_GT(saved_samples_->frames(), 0);

    scoped_refptr<media::AudioBusRefCounted> samples_bus =
        ConvertSavedSamplesToSystemParams();
    client->DecodeSamples(expect_audible ? AUDIBLE : INAUDIBLE,
                          AudioBusToString(samples_bus), token_params);
    run_loop_->Run();
  }

  void InitCallback(bool success) {
    EXPECT_TRUE(success);
    initialized_ = true;
    ASSERT_TRUE(run_loop_);
    run_loop_->Quit();
  }

  void SamplesCallback(
      audio_modem::AudioType type,
      const std::string& token,
      const scoped_refptr<media::AudioBusRefCounted>& samples) {
    EXPECT_EQ(expected_token_, token);
    EXPECT_EQ(expected_audible_, type == AUDIBLE);
    saved_samples_ = samples;
    ASSERT_TRUE(run_loop_);
    run_loop_->Quit();
  }

  void TokensCallback(const std::vector<audio_modem::AudioToken>& tokens) {
    ASSERT_TRUE(run_loop_);
    run_loop_->Quit();

    EXPECT_EQ(expected_token_, tokens[0].token);
    EXPECT_EQ(expected_audible_, tokens[0].audible);
  }

 private:
  scoped_refptr<media::AudioBusRefCounted> ConvertSavedSamplesToSystemParams() {
    int new_size =
        saved_samples_->frames() *
        std::ceil(static_cast<double>(default_params_.sample_rate()) /
                  coder_params_.sample_rate());
    new_size =
        std::ceil(static_cast<double>(new_size) / converter_->ChunkSize()) *
        converter_->ChunkSize();

    scoped_refptr<media::AudioBusRefCounted> converted_samples =
        media::AudioBusRefCounted::Create(default_params_.channels(), new_size);

    // Convert our single channel samples to two channel. Decode samples
    // expects 2 channel data.
    saved_samples_stereo_ =
        media::AudioBusRefCounted::Create(2, saved_samples_->frames());
    memcpy(saved_samples_stereo_->channel(0), saved_samples_->channel(0),
           sizeof(float) * saved_samples_->frames());
    memcpy(saved_samples_stereo_->channel(1), saved_samples_->channel(0),
           sizeof(float) * saved_samples_->frames());

    saved_samples_index_ = 0;
    converter_->Convert(converted_samples.get());

    return converted_samples;
  }

  // AudioConverter::InputCallback overrides:
  double ProvideInput(media::AudioBus* dest,
                      base::TimeDelta /* buffer_delay */) override {
    // Copy any saved samples we have to the output bus.
    const int remaining_frames =
        saved_samples_->frames() - saved_samples_index_;
    const int frames_to_copy = std::min(remaining_frames, dest->frames());
    saved_samples_stereo_->CopyPartialFramesTo(saved_samples_index_,
                                               frames_to_copy, 0, dest);
    saved_samples_index_ += frames_to_copy;

    // Pad any remaining space with zeroes.
    if (remaining_frames < dest->frames()) {
      dest->ZeroFramesPartial(remaining_frames,
                              dest->frames() - remaining_frames);
    }

    // Return the volume level.
    return 1.0;
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  bool initialized_;

  std::string expected_token_;
  bool expected_audible_;

  scoped_refptr<media::AudioBusRefCounted> saved_samples_;
  scoped_refptr<media::AudioBusRefCounted> saved_samples_stereo_;
  int saved_samples_index_;

  std::unique_ptr<media::AudioConverter> converter_;

  media::AudioParameters default_params_;
  media::AudioParameters coder_params_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWhispernetClientTest);
};

// These tests are irrelevant if NACL is disabled. See crbug.com/449198.
#if defined(DISABLE_NACL)
#define MAYBE_Initialize DISABLED_Initialize
#define MAYBE_EncodeAndDecode DISABLED_EncodeAndDecode
#define MAYBE_TokenLengths DISABLED_TokenLengths
#define MAYBE_Crc DISABLED_Crc
#define MAYBE_Parity DISABLED_Parity
#else
#define MAYBE_Initialize Initialize
#define MAYBE_EncodeAndDecode EncodeAndDecode
#define MAYBE_TokenLengths TokenLengths
#define MAYBE_Crc Crc
#define MAYBE_Parity Parity
#endif

// This test trips up ASAN on ChromeOS. See:
// https://code.google.com/p/address-sanitizer/issues/detail?id=189
#if defined(DISABLE_NACL) || defined(OS_CHROMEOS)
#define MAYBE_MultipleClients DISABLED_MultipleClients
#else
#define MAYBE_MultipleClients MultipleClients
#endif

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, MAYBE_Initialize) {
  InitializeWhispernet();
}

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, MAYBE_EncodeAndDecode) {
  std::unique_ptr<WhispernetClient> client(
      new ChromeWhispernetClient(browser()->profile()));
  client->Initialize(base::Bind(&IgnoreResult));
  SetupDecode();

  TokenParameters token_params[2];
  GetTokenParamsForLengths(kTokenLengths, token_params);

  EncodeTokenAndSaveSamples(client.get(), true, kSixZeros, token_params);
  DecodeSamplesAndVerifyToken(client.get(), true, kSixZeros, token_params);

  EncodeTokenAndSaveSamples(client.get(), false, kSixZeros, token_params);
  DecodeSamplesAndVerifyToken(client.get(), false, kSixZeros, token_params);
}

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, MAYBE_TokenLengths) {
  std::unique_ptr<WhispernetClient> client(
      new ChromeWhispernetClient(browser()->profile()));
  client->Initialize(base::Bind(&IgnoreResult));
  SetupDecode();

  const size_t kLongTokenLengths[2] = {8, 9};
  TokenParameters token_params[2];
  GetTokenParamsForLengths(kLongTokenLengths, token_params);

  EncodeTokenAndSaveSamples(client.get(), true, kEightZeros, token_params);
  DecodeSamplesAndVerifyToken(client.get(), true, kEightZeros, token_params);

  EncodeTokenAndSaveSamples(client.get(), false, kNineZeros, token_params);
  DecodeSamplesAndVerifyToken(client.get(), false, kNineZeros, token_params);
}

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, MAYBE_Crc) {
  std::unique_ptr<WhispernetClient> client(
      new ChromeWhispernetClient(browser()->profile()));
  client->Initialize(base::Bind(&IgnoreResult));
  SetupDecode();

  TokenParameters token_params[2];
  GetTokenParamsForLengths(kTokenLengths, token_params);
  token_params[0].crc = true;
  token_params[1].crc = true;

  EncodeTokenAndSaveSamples(client.get(), true, kSixZeros, token_params);
  DecodeSamplesAndVerifyToken(client.get(), true, kSixZeros, token_params);

  EncodeTokenAndSaveSamples(client.get(), false, kSixZeros, token_params);
  DecodeSamplesAndVerifyToken(client.get(), false, kSixZeros, token_params);
}

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, MAYBE_Parity) {
  std::unique_ptr<WhispernetClient> client(
      new ChromeWhispernetClient(browser()->profile()));
  client->Initialize(base::Bind(&IgnoreResult));
  SetupDecode();

  TokenParameters token_params[2];
  GetTokenParamsForLengths(kTokenLengths, token_params);
  token_params[0].parity = false;
  token_params[1].parity = false;

  EncodeTokenAndSaveSamples(client.get(), true, kSixZeros, token_params);
  DecodeSamplesAndVerifyToken(client.get(), true, kSixZeros, token_params);

  EncodeTokenAndSaveSamples(client.get(), false, kSixZeros, token_params);
  DecodeSamplesAndVerifyToken(client.get(), false, kSixZeros, token_params);
}

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, MAYBE_MultipleClients) {
  std::unique_ptr<WhispernetClient> client_1(
      new ChromeWhispernetClient(browser()->profile()));
  std::unique_ptr<WhispernetClient> client_2(
      new ChromeWhispernetClient(browser()->profile()));
  std::unique_ptr<WhispernetClient> client_3(
      new ChromeWhispernetClient(browser()->profile()));
  SetupDecode();

  TokenParameters token_params[2];
  GetTokenParamsForLengths(kTokenLengths, token_params);

  // Test concurrent initialization.
  client_1->Initialize(base::Bind(&IgnoreResult));
  client_2->Initialize(base::Bind(&IgnoreResult));

  EncodeTokenAndSaveSamples(client_1.get(), true, kSixZeros, token_params);
  DecodeSamplesAndVerifyToken(client_1.get(), true, kSixZeros, token_params);

  EncodeTokenAndSaveSamples(client_2.get(), false, kSixZeros, token_params);
  DecodeSamplesAndVerifyToken(client_2.get(), false, kSixZeros, token_params);

  // Test sequential initialization.
  client_3->Initialize(base::Bind(&IgnoreResult));

  EncodeTokenAndSaveSamples(client_3.get(), true, kSixZeros, token_params);
  DecodeSamplesAndVerifyToken(client_3.get(), true, kSixZeros, token_params);

  const size_t kLongTokenLengths[2] = {8, 9};
  GetTokenParamsForLengths(kLongTokenLengths, token_params);

  EncodeTokenAndSaveSamples(client_2.get(), true, kEightZeros, token_params);
  DecodeSamplesAndVerifyToken(client_2.get(), true, kEightZeros, token_params);
}
