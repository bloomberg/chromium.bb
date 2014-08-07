// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/copresence/chrome_whispernet_client.h"

#include "chrome/browser/extensions/api/copresence_private/copresence_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/api/copresence_private.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "grit/browser_resources.h"

namespace copresence {

// Once the CL for the copresence component lands, these constants will be
// need to be picked up from components/copresence/copresence_constants.h
const int kDefaultRepetitions = 5;
const float kDefaultSampleRate = 48000.0;
const int kDefaultBitsPerSample = 16;
const float kDefaultCarrierFrequency = 18500.0;
const int kDefaultChannels = 2;

}  // namespace copresence

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

void ChromeWhispernetClient::Initialize(const SuccessCallback& init_callback) {
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

// Fire an event to request a token encode.
void ChromeWhispernetClient::EncodeToken(const std::string& token) {
  DCHECK(extension_loaded_);
  DCHECK(browser_context_);
  DCHECK(extensions::EventRouter::Get(browser_context_));

  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::api::copresence_private::OnEncodeTokenRequest::kEventName,
      extensions::api::copresence_private::OnEncodeTokenRequest::Create(token),
      browser_context_));

  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(kWhispernetProxyExtensionId, event.Pass());
}

// Fire an event to request a decode for the given samples.
void ChromeWhispernetClient::DecodeSamples(const std::string& samples) {
  DCHECK(extension_loaded_);
  DCHECK(browser_context_);
  DCHECK(extensions::EventRouter::Get(browser_context_));

  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::api::copresence_private::OnDecodeSamplesRequest::kEventName,
      extensions::api::copresence_private::OnDecodeSamplesRequest::Create(
          samples),
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

ChromeWhispernetClient::TokensCallback
ChromeWhispernetClient::GetTokensCallback() {
  return tokens_callback_;
}

ChromeWhispernetClient::SamplesCallback
ChromeWhispernetClient::GetSamplesCallback() {
  return samples_callback_;
}

ChromeWhispernetClient::SuccessCallback
ChromeWhispernetClient::GetDetectBroadcastCallback() {
  return db_callback_;
}

ChromeWhispernetClient::SuccessCallback
ChromeWhispernetClient::GetInitializedCallback() {
  return extension_loaded_callback_;
}

// Private:

// Fire an event to initialize whispernet with the given parameters.
void ChromeWhispernetClient::InitializeWhispernet(
    const extensions::api::copresence_private::AudioParameters& params) {
  DCHECK(browser_context_);
  DCHECK(extensions::EventRouter::Get(browser_context_));

  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::api::copresence_private::OnInitialize::kEventName,
      extensions::api::copresence_private::OnInitialize::Create(params),
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

  // At this point, we have the same parameters for record and play. This
  // may change later though (ongoing discussion with research).
  extensions::api::copresence_private::AudioParameters params;
  params.play.sample_rate = copresence::kDefaultSampleRate;
  params.play.bits_per_sample = copresence::kDefaultBitsPerSample;
  params.play.carrier_frequency = copresence::kDefaultCarrierFrequency;
  params.play.repetitions = copresence::kDefaultRepetitions;

  params.record.sample_rate = copresence::kDefaultSampleRate;
  params.record.bits_per_sample = copresence::kDefaultBitsPerSample;
  params.record.carrier_frequency = copresence::kDefaultCarrierFrequency;
  params.record.channels = copresence::kDefaultChannels;

  InitializeWhispernet(params);
}
