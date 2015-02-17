// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/copresence/chrome_whispernet_client.h"

#include "base/stl_util.h"
#include "chrome/browser/copresence/chrome_whispernet_config.h"
#include "chrome/browser/extensions/api/copresence_private/copresence_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/api/copresence_private.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "grit/browser_resources.h"

using audio_modem::AUDIBLE;
using audio_modem::AudioType;
using audio_modem::BOTH;
using audio_modem::INAUDIBLE;
using audio_modem::SamplesCallback;
using audio_modem::SuccessCallback;
using audio_modem::TokensCallback;

namespace copresence_private = extensions::api::copresence_private;

namespace {

AudioParamData GetDefaultAudioConfig() {
  AudioParamData config_data = {};
  config_data.audio_dtmf.coder_sample_rate =
      config_data.audio_dsss.coder_sample_rate =
          audio_modem::kDefaultSampleRate;
  config_data.audio_dtmf.num_repetitions_to_play =
      config_data.audio_dsss.num_repetitions_to_play =
          audio_modem::kDefaultRepetitions;

  config_data.audio_dsss.upsampling_factor = audio_modem::kDefaultBitsPerSample;
  config_data.audio_dsss.desired_carrier_frequency =
      audio_modem::kDefaultCarrierFrequency;

  config_data.recording_channels = audio_modem::kDefaultChannels;

  return config_data;
}

}  // namespace

// static
const char ChromeWhispernetClient::kWhispernetProxyExtensionId[] =
    "bpfmnplchembfbdgieamdodgaencleal";

// Public:

ChromeWhispernetClient::ChromeWhispernetClient(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      event_router_(extensions::EventRouter::Get(browser_context)),
      extension_loaded_(false) {
  DCHECK(browser_context_);
  DCHECK(event_router_);
}

ChromeWhispernetClient::~ChromeWhispernetClient() {
}

void ChromeWhispernetClient::Initialize(
    const SuccessCallback& init_callback) {
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

  AudioConfiguration(GetDefaultAudioConfig());
}

void ChromeWhispernetClient::EncodeToken(const std::string& token_str,
                                         AudioType type) {
  copresence_private::EncodeTokenParameters params;
  params.token.token = token_str;
  params.token.audible = type == AUDIBLE;
  scoped_ptr<extensions::Event> event(new extensions::Event(
      copresence_private::OnEncodeTokenRequest::kEventName,
      copresence_private::OnEncodeTokenRequest::Create(params),
      browser_context_));

  SendEventIfLoaded(event.Pass());
}

void ChromeWhispernetClient::DecodeSamples(AudioType type,
                                           const std::string& samples,
                                           const size_t token_length[2]) {
  copresence_private::DecodeSamplesParameters params;
  params.samples.assign(samples.begin(), samples.end());
  params.decode_audible =
      type == AUDIBLE || type == BOTH;
  params.decode_inaudible =
      type == INAUDIBLE || type == BOTH;
  params.audible_token_length = token_length[AUDIBLE];
  params.inaudible_token_length = token_length[INAUDIBLE];

  scoped_ptr<extensions::Event> event(new extensions::Event(
      copresence_private::OnDecodeSamplesRequest::kEventName,
      copresence_private::OnDecodeSamplesRequest::Create(
          params),
      browser_context_));

  SendEventIfLoaded(event.Pass());
}

void ChromeWhispernetClient::DetectBroadcast() {
  scoped_ptr<extensions::Event> event(new extensions::Event(
      copresence_private::OnDetectBroadcastRequest::kEventName,
      make_scoped_ptr(new base::ListValue()),
      browser_context_));

  SendEventIfLoaded(event.Pass());
}

void ChromeWhispernetClient::RegisterTokensCallback(
    const TokensCallback& tokens_callback) {
  tokens_callback_ = tokens_callback;
}

void ChromeWhispernetClient::RegisterSamplesCallback(
    const SamplesCallback& samples_callback) {
  samples_callback_ = samples_callback;
}

void ChromeWhispernetClient::RegisterDetectBroadcastCallback(
    const SuccessCallback& db_callback) {
  db_callback_ = db_callback;
}

TokensCallback ChromeWhispernetClient::GetTokensCallback() {
  return tokens_callback_;
}

SamplesCallback ChromeWhispernetClient::GetSamplesCallback() {
  return samples_callback_;
}

SuccessCallback ChromeWhispernetClient::GetDetectBroadcastCallback() {
  return db_callback_;
}

SuccessCallback ChromeWhispernetClient::GetInitializedCallback() {
  return extension_loaded_callback_;
}

// Private:

void ChromeWhispernetClient::AudioConfiguration(const AudioParamData& params) {
  copresence_private::AudioParameters audio_params;

  // We serialize AudioConfigData to a string and send it to the whispernet
  // nacl wrapper.
  const size_t params_size = sizeof(params);
  audio_params.param_data.resize(params_size);
  memcpy(vector_as_array(&audio_params.param_data), &params, params_size);

  scoped_ptr<extensions::Event> event(new extensions::Event(
      copresence_private::OnConfigAudio::kEventName,
      copresence_private::OnConfigAudio::Create(audio_params),
      browser_context_));

  SendEventIfLoaded(event.Pass());
}

void ChromeWhispernetClient::SendEventIfLoaded(
    scoped_ptr<extensions::Event> event) {
  if (extension_loaded_) {
    event_router_->DispatchEventToExtension(kWhispernetProxyExtensionId,
                                            event.Pass());
  } else {
    DVLOG(2) << "Queueing event: " << event->event_name;
    queued_events_.push_back(event.release());
  }
}

void ChromeWhispernetClient::OnExtensionLoaded(bool success) {
  if (!init_callback_.is_null())
    init_callback_.Run(success);

  DVLOG(3) << "Sending " << queued_events_.size()
           << " queued requests to whispernet";

  // In this loop, ownership of each Event is passed to a scoped_ptr instead.
  // Thus we can just discard the pointers at the end.
  for (extensions::Event* event : queued_events_) {
    event_router_->DispatchEventToExtension(kWhispernetProxyExtensionId,
                                            make_scoped_ptr(event));
  }
  queued_events_.weak_clear();

  extension_loaded_ = true;
}
