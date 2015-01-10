// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/copresence/chrome_whispernet_client.h"

#include "base/stl_util.h"
#include "chrome/browser/extensions/api/copresence_private/copresence_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/api/copresence_private.h"
#include "components/copresence/public/copresence_constants.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "grit/browser_resources.h"

namespace {

copresence::config::AudioParamData GetDefaultAudioConfig() {
  copresence::config::AudioParamData config_data = {};
  config_data.audio_dtmf.coder_sample_rate =
      config_data.audio_dsss.coder_sample_rate = copresence::kDefaultSampleRate;
  config_data.audio_dtmf.num_repetitions_to_play =
      config_data.audio_dsss.num_repetitions_to_play =
          copresence::kDefaultRepetitions;

  config_data.audio_dsss.upsampling_factor = copresence::kDefaultBitsPerSample;
  config_data.audio_dsss.desired_carrier_frequency =
      copresence::kDefaultCarrierFrequency;

  config_data.recording_channels = copresence::kDefaultChannels;

  return config_data;
}

}  // namespace

// static
const char ChromeWhispernetClient::kWhispernetProxyExtensionId[] =
    "bpfmnplchembfbdgieamdodgaencleal";

// Public:

ChromeWhispernetClient::ChromeWhispernetClient(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), extension_loaded_(false) {
}

ChromeWhispernetClient::~ChromeWhispernetClient() {
}

void ChromeWhispernetClient::Initialize(
    const copresence::SuccessCallback& init_callback) {
  DVLOG(3) << "Initializing whispernet proxy client.";
  init_callback_ = init_callback;

  extensions::ExtensionSystem* es =
      extensions::ExtensionSystem::Get(browser_context_);
  DCHECK(es);
  ExtensionService* service = es->extension_service();
  DCHECK(service);
  extensions::ComponentLoader* loader = service->component_loader();
  DCHECK(loader);

  // This callback is cancelled in Shutdown().
  extension_loaded_callback_ = base::Bind(
      &ChromeWhispernetClient::OnExtensionLoaded, base::Unretained(this));

  if (!loader->Exists(kWhispernetProxyExtensionId)) {
    DVLOG(3) << "Loading Whispernet proxy.";
    loader->Add(IDR_WHISPERNET_PROXY_MANIFEST,
                base::FilePath(FILE_PATH_LITERAL("whispernet_proxy")));
  } else {
    init_callback_.Run(true);
  }
}

void ChromeWhispernetClient::Shutdown() {
  extension_loaded_callback_.Reset();
  init_callback_.Reset();
  tokens_callback_.Reset();
  samples_callback_.Reset();
  db_callback_.Reset();
}

void AudioConfiguration(const copresence::config::AudioParamData& data,
                        const copresence::SuccessCallback& init_callback) {
}

void ChromeWhispernetClient::EncodeToken(const std::string& token_str,
                                         copresence::AudioType type) {
  DCHECK(extension_loaded_);
  DCHECK(browser_context_);
  DCHECK(extensions::EventRouter::Get(browser_context_));

  extensions::api::copresence_private::EncodeTokenParameters params;
  params.token.token = token_str;
  params.token.audible = type == copresence::AUDIBLE;
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::api::copresence_private::OnEncodeTokenRequest::kEventName,
      extensions::api::copresence_private::OnEncodeTokenRequest::Create(params),
      browser_context_));

  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(kWhispernetProxyExtensionId, event.Pass());
}

void ChromeWhispernetClient::DecodeSamples(copresence::AudioType type,
                                           const std::string& samples,
                                           const size_t token_length[2]) {
  DCHECK(extension_loaded_);
  DCHECK(browser_context_);
  DCHECK(extensions::EventRouter::Get(browser_context_));

  extensions::api::copresence_private::DecodeSamplesParameters params;
  params.samples = samples;
  params.decode_audible =
      type == copresence::AUDIBLE || type == copresence::BOTH;
  params.decode_inaudible =
      type == copresence::INAUDIBLE || type == copresence::BOTH;
  params.audible_token_length = token_length[copresence::AUDIBLE];
  params.inaudible_token_length = token_length[copresence::INAUDIBLE];

  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::api::copresence_private::OnDecodeSamplesRequest::kEventName,
      extensions::api::copresence_private::OnDecodeSamplesRequest::Create(
          params),
      browser_context_));

  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(kWhispernetProxyExtensionId, event.Pass());
}

void ChromeWhispernetClient::DetectBroadcast() {
  DCHECK(extension_loaded_);
  DCHECK(browser_context_);
  DCHECK(extensions::EventRouter::Get(browser_context_));

  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::api::copresence_private::OnDetectBroadcastRequest::kEventName,
      make_scoped_ptr(new base::ListValue()),
      browser_context_));

  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(kWhispernetProxyExtensionId, event.Pass());
}

void ChromeWhispernetClient::RegisterTokensCallback(
    const copresence::TokensCallback& tokens_callback) {
  tokens_callback_ = tokens_callback;
}

void ChromeWhispernetClient::RegisterSamplesCallback(
    const copresence::SamplesCallback& samples_callback) {
  samples_callback_ = samples_callback;
}

void ChromeWhispernetClient::RegisterDetectBroadcastCallback(
    const copresence::SuccessCallback& db_callback) {
  db_callback_ = db_callback;
}

copresence::TokensCallback ChromeWhispernetClient::GetTokensCallback() {
  return tokens_callback_;
}

copresence::SamplesCallback ChromeWhispernetClient::GetSamplesCallback() {
  return samples_callback_;
}

copresence::SuccessCallback
ChromeWhispernetClient::GetDetectBroadcastCallback() {
  return db_callback_;
}

copresence::SuccessCallback ChromeWhispernetClient::GetInitializedCallback() {
  return extension_loaded_callback_;
}

// Private:

void ChromeWhispernetClient::AudioConfiguration(
    const copresence::config::AudioParamData& params) {
  DCHECK(browser_context_);
  DCHECK(extensions::EventRouter::Get(browser_context_));

  extensions::api::copresence_private::AudioParameters audio_params;

  // We serialize AudioConfigData to a string and send it to the whispernet
  // nacl wrapper.
  const size_t params_size = sizeof(params);
  audio_params.param_data.resize(params_size);
  memcpy(string_as_array(&audio_params.param_data), &params, params_size);

  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::api::copresence_private::OnConfigAudio::kEventName,
      extensions::api::copresence_private::OnConfigAudio::Create(audio_params),
      browser_context_));

  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(kWhispernetProxyExtensionId, event.Pass());
}

void ChromeWhispernetClient::OnExtensionLoaded(bool success) {
  if (extension_loaded_) {
    if (!init_callback_.is_null())
      init_callback_.Run(success);
    return;
  }

  // Our extension just got loaded, initialize whispernet.
  extension_loaded_ = true;

  // This will fire another OnExtensionLoaded call once the initialization is
  // done, which means we've initialized for realz, hence call the init
  // callback.

  AudioConfiguration(GetDefaultAudioConfig());
}
