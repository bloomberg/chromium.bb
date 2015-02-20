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
#include "components/audio_modem/public/whispernet_client.h"
#include "media/base/audio_bus.h"

using audio_modem::WhispernetClient;
using audio_modem::AUDIBLE;
using audio_modem::INAUDIBLE;
using audio_modem::TokenParameters;

namespace {

// TODO(rkc): Add more varied test input.
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

class ChromeWhispernetClientTest : public ExtensionBrowserTest {
 protected:
  ChromeWhispernetClientTest()
      : initialized_(false), expected_audible_(false) {}

  ~ChromeWhispernetClientTest() override {}

  void InitializeWhispernet() {
    scoped_ptr<WhispernetClient> client(
        new ChromeWhispernetClient(browser()->profile()));
    client->Initialize(base::Bind(
        &ChromeWhispernetClientTest::InitCallback, base::Unretained(this)));

    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();

    EXPECT_TRUE(initialized_);
  }

  void EncodeTokenAndSaveSamples(WhispernetClient* client,
                                 bool audible,
                                 const std::string& token) {
    run_loop_.reset(new base::RunLoop());
    client->RegisterSamplesCallback(
        base::Bind(&ChromeWhispernetClientTest::SamplesCallback,
                   base::Unretained(this)));
    expected_token_ = token;
    expected_audible_ = audible;

    TokenParameters token_params[2];
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

    client->DecodeSamples(expect_audible ? AUDIBLE : INAUDIBLE,
                          AudioBusToString(samples_bus),
                          token_params);
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

  scoped_ptr<base::RunLoop> run_loop_;
  bool initialized_;

 private:
  std::string expected_token_;
  bool expected_audible_;
  scoped_refptr<media::AudioBusRefCounted> saved_samples_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWhispernetClientTest);
};

// These tests are irrelevant if NACL is disabled. See crbug.com/449198
#if defined(DISABLE_NACL)
#define MAYBE_Initialize DISABLED_Initialize
#define MAYBE_EncodeAndDecode DISABLED_EncodeAndDecode
#define MAYBE_TokenLengths DISABLED_TokenLengths
#define MAYBE_MultipleClients DISABLED_MultipleClients
#else
#define MAYBE_Initialize Initialize
#define MAYBE_EncodeAndDecode EncodeAndDecode
#define MAYBE_TokenLengths TokenLengths
#define MAYBE_MultipleClients MultipleClients
#endif

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, MAYBE_Initialize) {
  InitializeWhispernet();
}

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, MAYBE_EncodeAndDecode) {
  scoped_ptr<WhispernetClient> client(
      new ChromeWhispernetClient(browser()->profile()));
  client->Initialize(base::Bind(&IgnoreResult));

  TokenParameters token_params[2];
  GetTokenParamsForLengths(kTokenLengths, token_params);

  EncodeTokenAndSaveSamples(client.get(), true, kSixZeros);
  DecodeSamplesAndVerifyToken(client.get(), true, kSixZeros, token_params);

  EncodeTokenAndSaveSamples(client.get(), false, kSixZeros);
  DecodeSamplesAndVerifyToken(client.get(), false, kSixZeros, token_params);
}

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, MAYBE_TokenLengths) {
  scoped_ptr<WhispernetClient> client(
      new ChromeWhispernetClient(browser()->profile()));
  client->Initialize(base::Bind(&IgnoreResult));

  const size_t kLongTokenLengths[2] = {8, 9};
  TokenParameters token_params[2];
  GetTokenParamsForLengths(kLongTokenLengths, token_params);
  EncodeTokenAndSaveSamples(client.get(), true, kEightZeros);
  DecodeSamplesAndVerifyToken(client.get(), true, kEightZeros, token_params);

  EncodeTokenAndSaveSamples(client.get(), false, kNineZeros);
  DecodeSamplesAndVerifyToken(client.get(), false, kNineZeros, token_params);
}

IN_PROC_BROWSER_TEST_F(ChromeWhispernetClientTest, MAYBE_MultipleClients) {
  scoped_ptr<WhispernetClient> client_1(
      new ChromeWhispernetClient(browser()->profile()));
  scoped_ptr<WhispernetClient> client_2(
      new ChromeWhispernetClient(browser()->profile()));

  TokenParameters token_params[2];
  GetTokenParamsForLengths(kTokenLengths, token_params);

  client_1->Initialize(base::Bind(&IgnoreResult));

  EncodeTokenAndSaveSamples(client_1.get(), true, kSixZeros);
  DecodeSamplesAndVerifyToken(client_1.get(), true, kSixZeros, token_params);

  client_2->Initialize(base::Bind(&IgnoreResult));

  EncodeTokenAndSaveSamples(client_2.get(), true, kSixZeros);
  DecodeSamplesAndVerifyToken(client_2.get(), true, kSixZeros, token_params);
}

// TODO(ckehoe): Test crc and parity
// TODO(ckehoe): More multi-client testing
