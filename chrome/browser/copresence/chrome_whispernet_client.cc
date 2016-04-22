// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/copresence/chrome_whispernet_client.h"

#include <stddef.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/copresence/chrome_whispernet_config.h"
#include "chrome/browser/extensions/api/copresence_private/copresence_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/api/copresence_private.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "grit/browser_resources.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_parameters.h"

using audio_modem::AUDIBLE;
using audio_modem::AudioType;
using audio_modem::BOTH;
using audio_modem::INAUDIBLE;
using audio_modem::SamplesCallback;
using audio_modem::SuccessCallback;
using audio_modem::TokensCallback;
using audio_modem::TokenParameters;

using extensions::api::copresence_private::AudioParameters;
using extensions::api::copresence_private::DecodeSamplesParameters;
using extensions::api::copresence_private::EncodeTokenParameters;
using ApiTokenParams = extensions::api::copresence_private::TokenParameters;

namespace OnConfigAudio =
    extensions::api::copresence_private::OnConfigAudio;
namespace OnDecodeSamplesRequest =
    extensions::api::copresence_private::OnDecodeSamplesRequest;
namespace OnEncodeTokenRequest =
    extensions::api::copresence_private::OnEncodeTokenRequest;

using extensions::Event;

namespace {

AudioParamData GetDefaultAudioConfig() {
  media::AudioParameters params =
      media::AudioManager::Get()->GetInputStreamParameters(
          media::AudioManagerBase::kDefaultDeviceId);

  AudioParamData config_data = {};

  config_data.audio_dtmf.coder_sample_rate =
      config_data.audio_dsss.coder_sample_rate =
          audio_modem::kDefaultSampleRate;

  config_data.audio_dtmf.recording_sample_rate =
      config_data.audio_dsss.recording_sample_rate = params.sample_rate();

  config_data.audio_dtmf.num_repetitions_to_play =
      config_data.audio_dsss.num_repetitions_to_play =
          audio_modem::kDefaultRepetitions;

  config_data.audio_dsss.upsampling_factor = audio_modem::kDefaultBitsPerSample;
  config_data.audio_dsss.desired_carrier_frequency =
      audio_modem::kDefaultCarrierFrequency;

  config_data.recording_channels = params.channels();

  return config_data;
}

// ApiTokenParams is not copyable, so we must take it as an output argument.
// TODO(ckehoe): Pass protos to Whispernet to avoid all these conversions.
void ConvertTokenParams(const TokenParameters& in, ApiTokenParams* out) {
  out->length = in.length;
  out->crc = in.crc;
  out->parity = in.parity;
}

}  // namespace

// static
const char ChromeWhispernetClient::kWhispernetProxyExtensionId[] =
    "bpfmnplchembfbdgieamdodgaencleal";


// Public functions.

ChromeWhispernetClient::ChromeWhispernetClient(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      event_router_(extensions::EventRouter::Get(browser_context)),
      extension_loaded_(false) {
  DCHECK(browser_context_);
}

ChromeWhispernetClient::~ChromeWhispernetClient() {}

void ChromeWhispernetClient::Initialize(
    const SuccessCallback& init_callback) {
  DVLOG(3) << "Initializing whispernet proxy client.";

  DCHECK(!init_callback.is_null());
  init_callback_ = init_callback;

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser_context_)->extension_service();
  CHECK(extension_service);

  extensions::ComponentLoader* loader = extension_service->component_loader();
  CHECK(loader);
  if (!loader->Exists(kWhispernetProxyExtensionId)) {
    DVLOG(3) << "Loading Whispernet proxy.";
    loader->Add(IDR_WHISPERNET_PROXY_MANIFEST,
                base::FilePath(FILE_PATH_LITERAL("whispernet_proxy")));
  }

  client_id_ = extensions::CopresencePrivateService::GetFactoryInstance()
      ->Get(browser_context_)->RegisterWhispernetClient(this);
  AudioConfiguration(GetDefaultAudioConfig());
}

void ChromeWhispernetClient::EncodeToken(
    const std::string& token_str,
    AudioType type,
    const TokenParameters token_params[2]) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);

  EncodeTokenParameters params;
  params.token.token = token_str;
  params.token.audible = (type == AUDIBLE);
  ConvertTokenParams(token_params[type], &params.token_params);

  SendEventIfLoaded(base::WrapUnique(new Event(
      extensions::events::COPRESENCE_PRIVATE_ON_ENCODE_TOKEN_REQUEST,
      OnEncodeTokenRequest::kEventName,
      OnEncodeTokenRequest::Create(client_id_, params), browser_context_)));
}

void ChromeWhispernetClient::DecodeSamples(
    AudioType type,
    const std::string& samples,
    const TokenParameters token_params[2]) {
  DecodeSamplesParameters params;
  params.samples.assign(samples.begin(), samples.end());
  params.decode_audible = (type == AUDIBLE || type == BOTH);
  params.decode_inaudible = (type == INAUDIBLE || type == BOTH);
  ConvertTokenParams(token_params[AUDIBLE], &params.audible_token_params);
  ConvertTokenParams(token_params[INAUDIBLE], &params.inaudible_token_params);

  SendEventIfLoaded(base::WrapUnique(new Event(
      extensions::events::COPRESENCE_PRIVATE_ON_DECODE_SAMPLES_REQUEST,
      OnDecodeSamplesRequest::kEventName,
      OnDecodeSamplesRequest::Create(client_id_, params), browser_context_)));
}

void ChromeWhispernetClient::RegisterTokensCallback(
    const TokensCallback& tokens_callback) {
  tokens_callback_ = tokens_callback;
}

void ChromeWhispernetClient::RegisterSamplesCallback(
    const SamplesCallback& samples_callback) {
  samples_callback_ = samples_callback;
}

TokensCallback ChromeWhispernetClient::GetTokensCallback() {
  return tokens_callback_;
}

SamplesCallback ChromeWhispernetClient::GetSamplesCallback() {
  return samples_callback_;
}

SuccessCallback ChromeWhispernetClient::GetInitializedCallback() {
  return base::Bind(&ChromeWhispernetClient::OnExtensionLoaded,
                    base::Unretained(this));
}


// Private functions.

void ChromeWhispernetClient::AudioConfiguration(const AudioParamData& params) {
  AudioParameters audio_params;

  // We serialize AudioConfigData to a string and send it to the whispernet
  // nacl wrapper.
  const size_t params_size = sizeof(params);
  audio_params.param_data.resize(params_size);
  memcpy(audio_params.param_data.data(), &params, params_size);

  DVLOG(3) << "Configuring audio for client " << client_id_;
  SendEventIfLoaded(base::WrapUnique(new Event(
      extensions::events::COPRESENCE_PRIVATE_ON_CONFIG_AUDIO,
      OnConfigAudio::kEventName,
      OnConfigAudio::Create(client_id_, audio_params), browser_context_)));
}

void ChromeWhispernetClient::SendEventIfLoaded(
    std::unique_ptr<extensions::Event> event) {
  DCHECK(event_router_);

  if (extension_loaded_) {
    event_router_->DispatchEventToExtension(kWhispernetProxyExtensionId,
                                            std::move(event));
  } else {
    DVLOG(2) << "Queueing event " << event->event_name
             << " for client " << client_id_;
    queued_events_.push_back(event.release());
  }
}

void ChromeWhispernetClient::OnExtensionLoaded(bool success) {
  DCHECK(!init_callback_.is_null());
  init_callback_.Run(success);

  DVLOG(3) << "Sending " << queued_events_.size()
           << " queued requests to whispernet from client "
           << client_id_;

  // In this loop, ownership of each Event is passed to a scoped_ptr instead.
  // Thus we can just discard the pointers at the end.
  DCHECK(event_router_);
  for (Event* event : queued_events_) {
    event_router_->DispatchEventToExtension(kWhispernetProxyExtensionId,
                                            base::WrapUnique(event));
  }
  queued_events_.weak_clear();

  extension_loaded_ = true;
}
